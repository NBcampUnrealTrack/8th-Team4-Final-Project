#include "Customization/AppearanceLocalPlayerSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

namespace
{
FString MetadataPath(const FString& Service, const FString& Account, FName Class)
{
    return FPaths::ProjectSavedDir() / TEXT("SaveGames/AppearanceSync") / FCustomizationStorage::SlotFor(Service, Account, Class.ToString());
}
bool SaveMetadata(const FString& Path, const FString& Hash)
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    if (!FFileHelper::SaveStringToFile(Hash, *(Path + TEXT(".tmp")))) return false;
    return IFileManager::Get().Move(*Path, *(Path + TEXT(".tmp")), true, true);
}
}

void UAppearanceLocalPlayerSubsystem::RefreshIdentity()
{
    FString NewService, NewAccount;
    if (const ULocalPlayer* Player = GetLocalPlayer())
    {
        const auto Id = Player->GetPreferredUniqueNetId().GetUniqueNetId();
        if (Id.IsValid() && Id->IsValid() && Id->GetType().ToString().Equals(TEXT("STEAM"), ESearchCase::IgnoreCase))
        {
            NewService = TEXT("STEAM");
            NewAccount = Id->ToString();
        }
    }
#if WITH_EDITOR
    if (NewAccount.IsEmpty()) { NewService = TEXT("LOCAL_TEST"); NewAccount = TEXT("EditorPreview"); }
#endif
    if (Service == NewService && Account == NewAccount) return;
    Service = NewService; Account = NewAccount; ++IdentityGeneration;
    Cloud.Reset(); FailedLocalSaves.Reset();
    if (Service != TEXT("STEAM")) return;
    if (!Backend) Backend = CreateSteamAppearanceBackend();
    const uint32 Generation = IdentityGeneration;
    const FString OwnerService = Service, OwnerAccount = Account;
    TWeakObjectPtr<UAppearanceLocalPlayerSubsystem> Weak(this);
    FAppearanceLocalIO IO;
    IO.IsCurrent = [Weak, Generation]()
    {
        if (!Weak.IsValid()) return false;
        Weak->RefreshIdentity();
        return Weak->IdentityGeneration == Generation;
    };
    IO.Load = [OwnerService, OwnerAccount](FName Class, FCustomizationAppearance& Out) { return FCustomizationStorage::Load(OwnerService, OwnerAccount, Class.ToString(), Out); };
    IO.Save = [OwnerService, OwnerAccount](FName Class, const FCustomizationAppearance& In) { return FCustomizationStorage::Save(OwnerService, OwnerAccount, Class.ToString(), In); };
    IO.LoadBase = [OwnerService, OwnerAccount](FName Class)
    {
        FString Hash; FFileHelper::LoadFileToString(Hash, *(MetadataPath(OwnerService, OwnerAccount, Class) + TEXT(".base")));
        return Hash.Len() == 32 ? Hash : FString();
    };
    IO.SaveBase = [OwnerService, OwnerAccount](FName Class, const FString& Hash) { return SaveMetadata(MetadataPath(OwnerService, OwnerAccount, Class) + TEXT(".base"), Hash); };
    IO.SaveConflict = [OwnerService, OwnerAccount](FName Class, const TArray<uint8>& Bytes)
    {
        FCustomizationAppearance Data;
        if (!AppearanceCloud::Decode(OwnerAccount, Class, Bytes, Data)) return false;
        const FString Path = MetadataPath(OwnerService, OwnerAccount, Class) + TEXT(".conflict.") + AppearanceCloud::Hash(Data) + TEXT(".sav");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        return FFileHelper::SaveArrayToFile(Bytes, *Path);
    };
    Cloud = MakeShared<FAppearanceCloudStore>(OwnerAccount, Backend, MoveTemp(IO));
    // Gameplay also reaches this path; synchronization must not require a studio visit.
    for (FName Class : {FName(TEXT("Ranger")), FName(TEXT("Trooper")), FName(TEXT("Vanguard"))}) Cloud->Refresh(Class);
}

ECustomizationLoadResult UAppearanceLocalPlayerSubsystem::LoadSavedAppearance(FName ClassId, FCustomizationAppearance& Out)
{
    RefreshIdentity();
    return FCustomizationStorage::Load(Service, Account, ClassId.ToString(), Out);
}

bool UAppearanceLocalPlayerSubsystem::SaveAppearance(FName ClassId, const FCustomizationAppearance& Data)
{
    RefreshIdentity();
    if (!FCustomizationStorage::Save(Service, Account, ClassId.ToString(), Data)) { FailedLocalSaves.Add(ClassId); return false; }
    FailedLocalSaves.Remove(ClassId);
    if (Cloud) Cloud->Refresh(ClassId);
    return true;
}

void UAppearanceLocalPlayerSubsystem::RefreshCloud()
{
    RefreshIdentity();
    if (Cloud) for (FName Class : {FName(TEXT("Ranger")), FName(TEXT("Trooper")), FName(TEXT("Vanguard"))})
        if (Cloud->Status(Class) != EAppearanceSaveStatus::Pending) Cloud->Refresh(Class);
}
void UAppearanceLocalPlayerSubsystem::ResolveConflict(FName ClassId, bool UseLocal)
{
    RefreshIdentity();
    if (Cloud) Cloud->Resolve(ClassId, UseLocal);
}
EAppearanceSaveStatus UAppearanceLocalPlayerSubsystem::GetSaveStatus(FName Class) const
{
    if (FailedLocalSaves.Contains(Class)) return EAppearanceSaveStatus::LocalFailed;
    return Cloud ? Cloud->Status(Class) : EAppearanceSaveStatus::LocalOnly;
}
FString UAppearanceLocalPlayerSubsystem::GetStatusText(FName Class) const
{
    switch (GetSaveStatus(Class))
    {
    case EAppearanceSaveStatus::Pending: return TEXT("클라우드 동기화 중 · 플레이는 PC 저장 사용");
    case EAppearanceSaveStatus::Synced: return TEXT("PC 저장 · Steam Cloud 요청 처리 완료");
    case EAppearanceSaveStatus::RestartRequired: return TEXT("클라우드 응답 없음 · 게임 재시작 필요 · PC 저장 유지");
    case EAppearanceSaveStatus::Conflict: return TEXT("PC/클라우드 저장이 다릅니다 · 사용할 사본 선택");
    case EAppearanceSaveStatus::CloudFailed: return TEXT("클라우드 동기화 실패 · 유효한 PC 저장 유지");
    case EAppearanceSaveStatus::LocalFailed: return TEXT("PC 저장/동기화 실패 · 기존 파일 유지");
    default: return TEXT("PC 로컬 저장 모드");
    }
}
void UAppearanceLocalPlayerSubsystem::Deinitialize()
{
    ++IdentityGeneration; Cloud.Reset(); Backend.Reset();
    Super::Deinitialize();
}
