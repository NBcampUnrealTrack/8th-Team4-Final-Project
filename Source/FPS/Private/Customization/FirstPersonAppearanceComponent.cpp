#include "Customization/FirstPersonAppearanceComponent.h"
#include "Character/ShooterCharacter.h"
#include "Player/ShooterPlayerState.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
const FName SkinTint(TEXT("Basecolor Global Multiply Post-Bake"));
const FName SkinValue(TEXT("Basecolor Global Value Post-Bake"));
const FName ClothTint(TEXT("ClassPartTint"));
const FName ClothValue(TEXT("ClassPartValue"));
const FName ClothOriginal(TEXT("ClassUseOriginal"));

FName ResolveClass(const AActor* Owner)
{
    for (const auto& Row : GetDefault<UDominationClassSettings>()->CombatClasses)
    {
        if (!Row.PawnClass.Get() || !Owner->IsA(Row.PawnClass.Get())) continue;
        switch (Row.CombatClass)
        {
        case EDominationCombatClass::Trooper: return TEXT("Trooper");
        case EDominationCombatClass::Ranger: return TEXT("Ranger");
        case EDominationCombatClass::Vanguard: return TEXT("Vanguard");
        default: return NAME_None;
        }
    }
    return NAME_None;
}

bool MatchesDriver(const USkeletalMesh* Mesh, const USkeletalMesh* Replacement)
{
    if (!Mesh || !Replacement) return false;
    const auto& A = Mesh->GetRefSkeleton();
    const auto& B = Replacement->GetRefSkeleton();
    // Both original arms and the tactical rig share the first 342 bones.
    // The replacement has no weights on the four tactical weapon helpers.
    if (A.GetRawBoneNum() != 342 && A.GetRawBoneNum() != 346) return false;
    if (B.GetRawBoneNum() != 346) return false;
    for (int32 Index = 0; Index < A.GetRawBoneNum(); ++Index)
        if (A.GetBoneName(Index) != B.GetBoneName(Index)
            || A.GetParentIndex(Index) != B.GetParentIndex(Index)
            || !A.GetRawRefBonePose()[Index].Equals(B.GetRawRefBonePose()[Index], .001f)) return false;
    return true;
}

int32 FindSlot(const USkeletalMesh* Mesh, FName Name)
{
    return Mesh->GetMaterials().IndexOfByPredicate([Name](const FSkeletalMaterial& Material) { return Material.MaterialSlotName == Name; });
}

bool MaterialContract(const USkeletalMesh* Mesh)
{
    const int32 SkinIndex = FindSlot(Mesh, TEXT("FP_Skin"));
    const int32 TopIndex = FindSlot(Mesh, TEXT("FP_Top"));
    const auto& Materials = Mesh->GetMaterials();
    if (!Materials.IsValidIndex(SkinIndex) || !Materials.IsValidIndex(TopIndex) || SkinIndex == TopIndex) return false;
    const UMaterialInterface* SkinBase = Materials[SkinIndex].MaterialInterface;
    const UMaterialInterface* TopBase = Materials[TopIndex].MaterialInterface;
    FLinearColor Color; float Value;
    return SkinBase && TopBase
        && SkinBase->GetVectorParameterValue(FMaterialParameterInfo(SkinTint), Color)
        && SkinBase->GetScalarParameterValue(FMaterialParameterInfo(SkinValue), Value)
        && TopBase->GetVectorParameterValue(FMaterialParameterInfo(ClothTint), Color)
        && TopBase->GetScalarParameterValue(FMaterialParameterInfo(ClothValue), Value)
        && TopBase->GetScalarParameterValue(FMaterialParameterInfo(ClothOriginal), Value);
}
}

UFirstPersonAppearanceComponent::UFirstPersonAppearanceComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = .25f;
    ClassVisuals.Add(TEXT("Trooper"), TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
        TEXT("/Game/FPS/Customizable/Character/FirstPerson/Trooper/SKM_FP_Trooper_ClothedArms.SKM_FP_Trooper_ClothedArms"))));
    ClassVisuals.Add(TEXT("Ranger"), TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
        TEXT("/Game/FPS/Customizable/Character/FirstPerson/Ranger/SKM_FP_Ranger_ClothedArms.SKM_FP_Ranger_ClothedArms"))));
    ClassVisuals.Add(TEXT("Vanguard"), TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
        TEXT("/Game/FPS/Customizable/Character/FirstPerson/Vanguard/SKM_FP_Vanguard_ClothedArms.SKM_FP_Vanguard_ClothedArms"))));
}

void UFirstPersonAppearanceComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) { SetComponentTickEnabled(false); return; }
    RefreshFromPlayerState();
}

void UFirstPersonAppearanceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Restore();
    Super::EndPlay(Reason);
}

void UFirstPersonAppearanceComponent::TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Tick, Function);
    RefreshFromPlayerState();
}

void UFirstPersonAppearanceComponent::Restore()
{
    if (IsValid(Visual)) { Visual->SetVisibility(false); Visual->DestroyComponent(); }
    if (IsValid(Driver))
    {
        Driver->SetOwnerNoSee(bOriginalOwnerNoSee);
        Driver->VisibilityBasedAnimTickOption = OriginalTickOption;
    }
    Visual = nullptr; Driver = nullptr; AppliedMesh = nullptr; AppliedVisual = nullptr;
    Skin = nullptr; Top = nullptr; Gloves = nullptr;
    SkinSlot = INDEX_NONE; TopSlot = INDEX_NONE; GlovesSlot = INDEX_NONE;
}

void UFirstPersonAppearanceComponent::UpdateColors(const FCustomizationAppearance* Appearance)
{
    if (!Visual || !AppliedVisual) return;
    UMaterialInterface* SkinBase = AppliedVisual->GetMaterials()[SkinSlot].MaterialInterface;
    UMaterialInterface* TopBase = AppliedVisual->GetMaterials()[TopSlot].MaterialInterface;
    if (!Appearance)
    {
        if (Skin) Visual->SetMaterial(SkinSlot, SkinBase);
        if (Top) Visual->SetMaterial(TopSlot, TopBase);
        if (Gloves) Visual->SetMaterial(GlovesSlot, AppliedVisual->GetMaterials()[GlovesSlot].MaterialInterface);
        Skin = nullptr; Top = nullptr; Gloves = nullptr;
        return;
    }
    if (!Skin)
    {
        SkinBase->GetVectorParameterValue(FMaterialParameterInfo(SkinTint), OriginalTint);
        SkinBase->GetScalarParameterValue(FMaterialParameterInfo(SkinValue), OriginalValue);
        Skin = UMaterialInstanceDynamic::Create(SkinBase, this);
        Visual->SetMaterial(SkinSlot, Skin);
    }
    if (!Top)
    {
        Top = UMaterialInstanceDynamic::Create(TopBase, this);
        Visual->SetMaterial(TopSlot, Top);
    }
    Skin->SetVectorParameterValue(SkinTint, OriginalTint * Appearance->SkinColor);
    Skin->SetScalarParameterValue(SkinValue, OriginalValue * Appearance->SkinValue);
    Top->SetVectorParameterValue(ClothTint, Appearance->Colors[0]);
    Top->SetScalarParameterValue(ClothValue, Appearance->Values[0]);
    Top->SetScalarParameterValue(ClothOriginal, Appearance->Originals[0]);
    if (GlovesSlot != INDEX_NONE)
    {
        if (!Gloves)
        {
            Gloves = UMaterialInstanceDynamic::Create(AppliedVisual->GetMaterials()[GlovesSlot].MaterialInterface, this);
            Visual->SetMaterial(GlovesSlot, Gloves);
        }
        const int32 PartIndex = FCustomizationAppearance::Parts().IndexOfByKey(TEXT("Gloves"));
        if (Appearance->Colors.IsValidIndex(PartIndex) && Appearance->Values.IsValidIndex(PartIndex)
            && Appearance->Originals.IsValidIndex(PartIndex))
        {
            Gloves->SetVectorParameterValue(ClothTint, Appearance->Colors[PartIndex]);
            Gloves->SetScalarParameterValue(ClothValue, Appearance->Values[PartIndex]);
            Gloves->SetScalarParameterValue(ClothOriginal, Appearance->Originals[PartIndex]);
        }
    }
}

void UFirstPersonAppearanceComponent::RefreshFromPlayerState()
{
    auto* Character = Cast<AShooterCharacter>(GetOwner());
    if (!Character || GetNetMode() == NM_DedicatedServer || !Character->IsPlayerControlled()
        || !Character->IsLocallyControlled()) { Restore(); return; }
    const FName ClassId = ResolveClass(Character);
    auto* Selection = ClassVisuals.Find(ClassId);
    auto* Current = Character->GetMesh1P_Implementation();
    if (!Selection || Selection->IsNull() || !Current || !Current->GetSkeletalMeshAsset()) { Restore(); return; }
    USkeletalMesh* Asset = Selection->LoadSynchronous();
    if (Driver != Current || AppliedMesh != Current->GetSkeletalMeshAsset() || AppliedVisual != Asset || !IsValid(Visual))
    {
        Restore();
        // Validate the full replacement before hiding any part of the fallback.
        if (!MatchesDriver(Current->GetSkeletalMeshAsset(), Asset) || !MaterialContract(Asset)) return;
        Driver = Current; AppliedMesh = Current->GetSkeletalMeshAsset(); AppliedVisual = Asset;
        bOriginalOwnerNoSee = Driver->bOwnerNoSee;
        OriginalTickOption = Driver->VisibilityBasedAnimTickOption;
        SkinSlot = FindSlot(Asset, TEXT("FP_Skin"));
        TopSlot = FindSlot(Asset, TEXT("FP_Top"));
        GlovesSlot = FindSlot(Asset, TEXT("FP_Gloves"));
        Visual = NewObject<USkeletalMeshComponent>(Character);
        Character->AddInstanceComponent(Visual);
        Visual->ComponentTags.Add(TEXT("FirstPersonAppearance"));
        Visual->ComponentTags.Add(TEXT("AppearanceNoOutline"));
        Visual->SetupAttachment(Driver);
        Visual->SetSkeletalMeshAsset(Asset);
        Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Visual->SetOnlyOwnerSee(true);
        Visual->SetOwnerNoSee(false);
        Visual->SetCastShadow(false);
        Visual->bReceivesDecals = false;
        Visual->SetLeaderPoseComponent(Driver);
        Visual->RegisterComponent();
        if (!Visual->IsRegistered()) { Restore(); return; }
        Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        // OwnerNoSee affects only the driver's surface, not its attached weapon.
        Driver->SetOwnerNoSee(true);
    }
    const auto* State = Character->GetPlayerState<AShooterPlayerState>();
    const auto* Saved = State ? State->FindAppearance(ClassId) : nullptr;
    UpdateColors(Saved && Saved->Appearance.ValidateForClass(ClassId.ToString()) ? &Saved->Appearance : nullptr);
    UpdateVisibility();
}

void UFirstPersonAppearanceComponent::UpdateVisibility()
{
    if (!Visual || !Driver) return;
    Visual->SetVisibility(bPresentationVisible && Driver->IsVisible());
    Visual->SetHiddenInGame(Driver->bHiddenInGame);
}

void UFirstPersonAppearanceComponent::SetPresentationVisible(bool bVisible)
{
    bPresentationVisible = bVisible;
    UpdateVisibility();
}
