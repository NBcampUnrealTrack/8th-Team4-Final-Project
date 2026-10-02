#include "Customization/GameplayAppearanceComponent.h"
#include "Customization/GameplayAppearanceConfig.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/AppearanceParameters.h"
#include "Customization/AppearancePoseAnimInstance.h"
#include "Customization/AppearanceBoneDependencyComponent.h"
#include "Character/ShooterCharacter.h"
#include "Player/ShooterPlayerState.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LODSyncComponent.h"
#include "Engine/SkeletalMesh.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "GroomComponent.h"
#include "GroomBindingAsset.h"
#include "GroomAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "Combat/RogueDeathPresentation.h"
#include "Weapon/WeaponBase.h"

USkeletalMeshComponent* UGameplayAppearanceComponent::GetReadyBody() const
{
    return AppliedRevision && IsValid(Body) && Body->GetSkeletalMeshAsset() ? Body.Get() : nullptr;
}

namespace
{
FSoftObjectPath GameplayAppearancePath(const FName ClassId)
{
    const FString PackagePath = TEXT("/Game/FPS/Customizable/Character/Gameplay/DA_Gameplay") + ClassId.ToString();
    return FSoftObjectPath(PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath));
}

void FollowPose(USkeletalMeshComponent* Target, USkeletalMeshComponent* Source, bool PreserveReferencePose = true)
{
    Target->SetAnimInstanceClass(UAppearancePoseAnimInstance::StaticClass());
    if (auto* Anim = Cast<UAppearancePoseAnimInstance>(Target->GetAnimInstance()))
    { Anim->PoseSource = Source; Anim->bPreserveReferencePose = PreserveReferencePose; }
    Target->AddTickPrerequisiteComponent(Source);
}
}

UGameplayAppearanceComponent::UGameplayAppearanceComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickInterval = .1f;
}

void UGameplayAppearanceComponent::AppendPreloadAssetPaths(const FName ClassId, TArray<FSoftObjectPath>& OutPaths) const
{
    if (ClassId.IsNone()) return;
    const FSoftObjectPath Path = GameplayAppearancePath(ClassId);
    // 외형 카탈로그가 없는 클래스는 기존 기본 외형을 사용한다.
    if (FPackageName::DoesPackageExist(Path.GetLongPackageName())) OutPaths.AddUnique(Path);
    else UE_LOG(LogTemp, Verbose, TEXT("[Preload] Optional appearance catalog unavailable: %s"), *Path.ToString());
}

FName UGameplayAppearanceComponent::ResolveClassId() const
{
    const auto* Settings = GetDefault<UDominationClassSettings>();
    for (const auto& Definition : Settings->CombatClasses)
    {
        if (!Definition.PawnClass.Get() || !GetOwner()->IsA(Definition.PawnClass.Get())) continue;
        switch (Definition.CombatClass)
        {
        case EDominationCombatClass::Ranger: return TEXT("Ranger");
        case EDominationCombatClass::Trooper: return TEXT("Trooper");
        case EDominationCombatClass::Vanguard: return TEXT("Vanguard");
        default: return NAME_None;
        }
    }
    return NAME_None;
}

void UGameplayAppearanceComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeAuthoredBoneDependencies();
    RefreshFromPlayerState();
}

void UGameplayAppearanceComponent::InitializeAuthoredBoneDependencies()
{
    if (GetNetMode() == NM_DedicatedServer) return;
    auto* Character = Cast<AShooterCharacter>(GetOwner());
    auto* Driver = Character ? Character->GetMesh() : nullptr;
    if (!Driver) return;

    TArray<USkeletalMeshComponent*> Parts;
    TArray<USkeletalMeshComponent*> Followers;
    Character->GetComponents(Parts);
    for (auto* Part : Parts)
    {
        if (Part == Driver || Part == Character->GetMesh1P_Implementation()
            || Part->ComponentHasTag(TEXT("FirstPersonAppearance"))) continue;
        if (Part->GetSkeletalMeshAsset() && Part->LeaderPoseComponent.Get() == Driver)
            Followers.Add(Part);
    }
    if (Followers.IsEmpty()) return;

    // Low may reduce the body's evaluated bones while authored clothing still
    // renders LOD0. Protect the default appearance too, before any replicated
    // customization arrives. Keep its pose ownership and geometry LOD unchanged.
    DriverBoneDependency = NewObject<UAppearanceBoneDependencyComponent>(Character, TEXT("AppearanceDriverBoneDependency"));
    Character->AddInstanceComponent(DriverBoneDependency);
    DriverBoneDependency->RegisterComponent();
    DriverBoneDependency->Configure(Driver, Followers);
}

void UGameplayAppearanceComponent::RefreshFromPlayerState()
{
    if (!HasBegunPlay() || GetNetMode() == NM_DedicatedServer) return;
    auto* Character = Cast<AShooterCharacter>(GetOwner());
    const auto* State = Character ? Character->GetPlayerState<AShooterPlayerState>() : nullptr;
    if (!State) return;
    const FName ClassId = ResolveClassId();
    if (const auto* Data = State->FindAppearance(ClassId)) RequestAppearance(*Data);
}

void UGameplayAppearanceComponent::RequestAppearance(const FReplicatedAppearance& Data)
{
    if (GetNetMode() == NM_DedicatedServer || Data.ClassId != ResolveClassId()
        || Data.Revision == 0 || !Data.Appearance.ValidateForClass(Data.ClassId.ToString())) return;
    if (Desired.ClassId == Data.ClassId && Desired.Revision == Data.Revision) return;
    Desired = Data;
    ++RequestSerial;
    bQueued = true;
    if (!bBusy) StartUpdate();
}

void UGameplayAppearanceComponent::EnsureVisualComponents()
{
    if (Body) return;
    auto* Character = CastChecked<AShooterCharacter>(GetOwner());
    auto* Driver = Character->GetMesh();
    TArray<USkeletalMeshComponent*> Meshes;
    Character->GetComponents(Meshes);
    for (auto* Mesh : Meshes)
        if (Mesh != Character->GetMesh1P_Implementation()
            && !Mesh->IsA<UAppearanceBoneDependencyComponent>()
            && !Mesh->ComponentHasTag(TEXT("FirstPersonAppearance"))) AuthoredVisuals.Add(Mesh);
    TArray<UGroomComponent*> OldGrooms;
    Character->GetComponents(OldGrooms);
    for (auto* Groom : OldGrooms) AuthoredVisuals.Add(Groom);
    auto MakeMesh = [&](FName Name)
    {
        auto* Mesh = NewObject<USkeletalMeshComponent>(Character, Name);
        Character->AddInstanceComponent(Mesh);
        Mesh->SetupAttachment(Driver);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetOwnerNoSee(Driver->bOwnerNoSee);
        Mesh->SetVisibility(false);
        Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Mesh->RegisterComponent();
        return Mesh;
    };
    Body = MakeMesh(TEXT("AppearanceBody"));
    Face = MakeMesh(TEXT("AppearanceFace"));
    Outfit = MakeMesh(TEXT("AppearanceOutfit"));
    BindingSurface = MakeMesh(TEXT("AppearanceBindingSurface"));
    // MetaHuman's face post-process copies its attached body's pose as well.
    // Attach to the selected generated body, not the original gameplay driver.
    BindingSurface->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform);
    BindingSurface->SetRenderInMainPass(false);
    BindingSurface->SetRenderInDepthPass(false);
    BindingSurface->ComponentTags.Add(TEXT("AppearanceNoOutline"));
    BindingSurface->SetCastShadow(false);
    BindingSurface->bVisibleInRayTracing = false;
    for (FName Name : {FName(TEXT("Hair")), FName(TEXT("Eyebrows")), FName(TEXT("Eyelashes")), FName(TEXT("Fuzz")), FName(TEXT("Mustache")), FName(TEXT("Beard"))})
    {
        auto* Groom = NewObject<UGroomComponent>(Character, FName(*(TEXT("Appearance") + Name.ToString())));
        Groom->ComponentTags.Add(Name);
        Character->AddInstanceComponent(Groom);
        Groom->SetupAttachment(BindingSurface);
        Groom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Groom->SetOwnerNoSee(Driver->bOwnerNoSee);
        Groom->SetVisibility(false);
        Groom->RegisterComponent();
        Grooms.Add(Groom);
    }
}

void UGameplayAppearanceComponent::RefreshLODSynchronization()
{
    auto* Character = CastChecked<AShooterCharacter>(GetOwner());
    auto* Driver = Character->GetMesh();
    auto* Sync = Character->FindComponentByClass<ULODSyncComponent>();
    if (!Sync)
    {
        Sync = NewObject<ULODSyncComponent>(Character, TEXT("AppearanceLODSync"));
        Character->AddInstanceComponent(Sync);
        Sync->RegisterComponent();
    }
    // The original MetaHuman components no longer render after customization.
    // Retain their authored mappings but let the generated visible parts drive LOD.
    for (auto& Entry : Sync->ComponentsToSync)
        if (Entry.SyncOption == ESyncOption::Drive) Entry.SyncOption = ESyncOption::Passive;
    auto Register = [&](FName Name, ESyncOption Option, const FLODMappingData* Mapping)
    {
        Sync->ComponentsToSync.RemoveAll([Name](const FComponentSync& Entry) { return Entry.Name == Name; });
        Sync->ComponentsToSync.Emplace(Name, Option);
        if (Mapping) Sync->CustomLODMapping.Add(Name, *Mapping);
        else Sync->CustomLODMapping.Remove(Name);
    };
    // MetaHuman face has eight LODs and standard body has four. One-LOD
    // generated parts clamp automatically. Preserve the authored body mapping.
    FLODMappingData BodyMap;
    BodyMap.Mapping = {0, 0, 1, 1, 2, 2, 3, 3};
    if (const auto* Existing = Sync->CustomLODMapping.Find(Driver->GetFName())) BodyMap = *Existing;
    else if (const auto* AuthoredBodyMap = Sync->CustomLODMapping.Find(TEXT("Body"))) BodyMap = *AuthoredBodyMap;
    Register(Driver->GetFName(), ESyncOption::Passive, &BodyMap);
    Register(Body->GetFName(), ESyncOption::Drive, &BodyMap);
    Register(Outfit->GetFName(), ESyncOption::Passive, &BodyMap);
    Register(BindingSurface->GetFName(), ESyncOption::Passive, nullptr);
    Register(Face->GetFName(), ESyncOption::Drive, nullptr);
    for (UGroomComponent* Groom : Grooms)
    {
        FLODMappingData GroomMap;
        const auto* Existing = Sync->CustomLODMapping.Find(Groom->ComponentTags[0]);
        if (Existing) GroomMap = *Existing;
        Register(Groom->GetFName(), ESyncOption::Passive, Existing ? &GroomMap : nullptr);
    }
    Sync->RefreshSyncComponents();
    Sync->UpdateLOD();
    auto MakeDependency = [&](FName Name)
    {
        auto* Component = NewObject<UAppearanceBoneDependencyComponent>(Character, Name);
        Character->AddInstanceComponent(Component);
        Component->RegisterComponent();
        return Component;
    };
    if (!DriverBoneDependency) DriverBoneDependency = MakeDependency(TEXT("AppearanceDriverBoneDependency"));
    if (!BodyBoneDependency) BodyBoneDependency = MakeDependency(TEXT("AppearanceBodyBoneDependency"));
    // Outfit may have only LOD0 while the body has four LODs. Request its bones
    // without locking either geometry or the face/grooms to LOD0.
    DriverBoneDependency->Configure(Driver, {Body, Outfit});
    BodyBoneDependency->Configure(Body, {Outfit});
}

void UGameplayAppearanceComponent::StartUpdate()
{
    bQueued = false;
    if (!Config)
    {
        const FSoftObjectPath Path = GameplayAppearancePath(Desired.ClassId);
        Config = LoadObject<UGameplayAppearanceConfig>(nullptr, *Path.ToString());
        Studio = Config ? Config->StudioConfig.LoadSynchronous() : nullptr;
    }
    if (!Config || !Studio || !Studio->CustomizableObject) { Failure = TEXT("Gameplay appearance catalog missing"); return; }
#if WITH_EDITOR
    if (!Studio->CustomizableObject->IsCompiled())
    {
        FCompileParams Params; Params.bAsync = false;
        Studio->CustomizableObject->Compile(Params);
    }
#endif
    if (!Studio->CustomizableObject->IsCompiled()) { Failure = TEXT("Gameplay CO not compiled/cooked"); return; }
    const FGameplayAppearanceBody* Definition = Config->Bodies.FindByPredicate([this](const auto& B){ return B.BodyType == Desired.Appearance.BodyType; });
    USkeletalMesh* SourceFace = Definition ? Definition->FaceMesh.LoadSynchronous() : nullptr;
    if (!SourceFace) { Failure = TEXT("Original binding face missing"); return; }
    Pending = Studio->CustomizableObject->CreateInstance();
    if (!ApplyAppearanceParameters(Pending, Studio, Desired.Appearance)) { Pending = nullptr; Failure = TEXT("Invalid appearance parameters"); return; }
    const uint32 Serial = RequestSerial;
    const FReplicatedAppearance Snapshot = Desired;
    bBusy = true;
    TWeakObjectPtr<UGameplayAppearanceComponent> Weak(this);
    FInstanceUpdateNativeDelegate Callback;
    Callback.AddLambda([Weak, Serial, Snapshot, SourceFace](const FUpdateContext& Context)
    {
        if (!Weak.IsValid()) return;
        auto* Self = Weak.Get();
        Self->bBusy = false;
        if (Serial != Self->RequestSerial)
        {
            Self->Pending = nullptr;
            if (Self->bQueued) Self->StartUpdate();
            return;
        }
        auto* Instance = Self->Pending.Get();
        bool Valid = Instance && (Context.UpdateResult == EUpdateResult::Success || Context.UpdateResult == EUpdateResult::Warning);
        const FName Names[] = {TEXT("Body"), TEXT("Face"), TEXT("Outfit")};
        TArray<USkeletalMesh*> Meshes;
        for (FName Name : Names)
        {
            auto* Mesh = Instance ? Instance->GetSkeletalMeshComponentSkeletalMesh(Name) : nullptr;
            Meshes.Add(Mesh);
            Valid &= Mesh != nullptr;
        }
        if (!Valid) { Self->Failure = TEXT("Mutable generation failed; previous appearance retained"); Self->Pending = nullptr; return; }
        // Verify every chosen groom against the actual authored binding surface before replacing anything.
        for (const auto& Choice : Self->Studio->Grooms)
        {
            if (Choice.BodyType != Snapshot.Appearance.BodyType) continue;
            if (Choice.Component == TEXT("Hair") && Choice.Id != Snapshot.Appearance.HairId) continue;
            if (!Choice.Groom || !Choice.Binding || !UGroomBindingAsset::IsCompatible(SourceFace, Choice.Binding, false)
                || !UGroomBindingAsset::IsCompatible(Choice.Groom, Choice.Binding, false)) Valid = false;
        }
        if (!Valid) { Self->Failure = TEXT("Groom binding is incompatible with selected face"); Self->Pending = nullptr; return; }
        Self->EnsureVisualComponents();
        auto* Character = CastChecked<AShooterCharacter>(Self->GetOwner());
        auto* Driver = Character->GetMesh();
        Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        const TArray<USkeletalMeshComponent*> Targets = {Self->Body, Self->Face, Self->Outfit};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            Targets[Index]->EmptyOverrideMaterials();
            Targets[Index]->SetDisablePostProcessBlueprint(true);
            Targets[Index]->SetSkeletalMesh(Meshes[Index]);
            const auto Materials = Instance->GetSkeletalMeshComponentOverrideMaterials(Names[Index]);
            for (int32 Slot = 0; Slot < Materials.Num(); ++Slot) Targets[Index]->SetMaterial(Slot, Materials[Slot]);
        }
        FollowPose(Self->Body, Driver);
        if (auto* Pose = Cast<UAppearancePoseAnimInstance>(Self->Body->GetAnimInstance()))
            Pose->bPreserveWeaponHandRelationship = true;
        FollowPose(Self->Outfit, Self->Body, false);
        Self->BindingSurface->SetSkeletalMesh(SourceFace);
        // Run the selected original face's post-process once, then copy the evaluated
        // facial pose/curves to the generated face. Never use another identity's DNA.
        Self->BindingSurface->SetDisablePostProcessBlueprint(false);
        Self->BindingSurface->SetOverridePostProcessAnimBP(SourceFace->GetPostProcessAnimBlueprint());
        // Copy matching body attachment bones in component space. Unmatched
        // facial bones keep this selected face's own reference pose and DNA.
        FollowPose(Self->BindingSurface, Self->Body, false);
        FollowPose(Self->Face, Self->BindingSurface, false);
        Self->BindingSurface->SetVisibility(true);
        for (UGroomComponent* Groom : Self->Grooms)
        {
            Groom->SetVisibility(false);
            Groom->SetGroomAsset(nullptr);
            Groom->EmptyOverrideMaterials();
            const FName Part = Groom->ComponentTags[0];
            for (const auto& Choice : Self->Studio->Grooms)
            {
                if (Choice.BodyType != Snapshot.Appearance.BodyType || Choice.Component != Part) continue;
                if (Part == TEXT("Hair") && Choice.Id != Snapshot.Appearance.HairId) continue;
                Groom->SetGroomAsset(Choice.Groom, Choice.Binding, true);
                Groom->SetVisibility(true);
                if (Part == TEXT("Hair") && !Snapshot.Appearance.bUseOriginalHairColor)
                {
                    const auto& Materials = Choice.Groom->GetHairGroupsMaterials();
                    for (int32 Slot = 0; Slot < Materials.Num(); ++Slot) if (Materials[Slot].Material)
                    {
                        auto* Dye = UMaterialInstanceDynamic::Create(Materials[Slot].Material, Groom);
                        Dye->SetVectorParameterValue(TEXT("hairDye"), Snapshot.Appearance.HairColor);
                        for (const TCHAR* Parameter : {TEXT("hairMelanin"), TEXT("hairRedness"), TEXT("MelaninVariationFine"), TEXT("MelaninVariationRough"), TEXT("RedVariation"), TEXT("Ombre"), TEXT("Highlights"), TEXT("Region"), TEXT("WhiteAmount")})
                            Dye->SetScalarParameterValue(Parameter, 0.f);
                        Groom->SetMaterial(Slot, Dye);
                    }
                }
                break;
            }
        }
        for (UPrimitiveComponent* Authored : Self->AuthoredVisuals)
        {
            Authored->SetRenderInMainPass(false); Authored->SetRenderInDepthPass(false); Authored->SetCastShadow(false);
            Authored->bVisibleInRayTracing = false;
            Authored->ComponentTags.AddUnique(TEXT("AppearanceNoOutline"));
            Authored->SetRenderCustomDepth(false);
        }
        Self->RefreshLODSynchronization();
        for (auto* Target : Targets) Target->SetVisibility(true);
        Self->Live = Self->Pending; Self->Pending = nullptr;
        Self->AppliedRevision = Snapshot.Revision; Self->Failure.Reset();
        if (AWeaponBase* Weapon = Character->GetCurrentWeapon_Implementation())
            Weapon->RefreshThirdPersonAttachment();
        Self->SetComponentTickEnabled(true);
        // An asynchronous Mutable update can finish after the character has died.
        RogueDeathPresentation::RefreshRagdollFollowers(*Character);
        Character->RefreshTeamOutline();
    });
    Pending->UpdateSkeletalMeshAsyncResult(Callback, true, true);
}

void UGameplayAppearanceComponent::TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Tick, Function);
    const auto* Character = Cast<AShooterCharacter>(GetOwner());
    if (!Character || !Body) return;
    const bool OwnerHidden = Character->GetMesh()->bOwnerNoSee;
    for (auto* Mesh : {Body.Get(), Face.Get(), Outfit.Get()}) Mesh->SetOwnerNoSee(OwnerHidden);
    for (UGroomComponent* Groom : Grooms) Groom->SetOwnerNoSee(OwnerHidden);
}

void UGameplayAppearanceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ++RequestSerial; bQueued = false;
    Pending = nullptr; Live = nullptr;
    Super::EndPlay(Reason);
}
