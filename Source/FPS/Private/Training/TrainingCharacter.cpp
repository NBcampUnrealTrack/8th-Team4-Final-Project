#include "Training/TrainingCharacter.h"

#include "AbilitySystemComponent.h"
#include "Combat/CombatHitZoneDefinition.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GAS/RogueAugmentAttributeSet.h"
#include "GAS/RogueHealthComponent.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GameplayTags.h"
#include "Training/TrainingSettings.h"

ATrainingCharacter::ATrainingCharacter()
{
    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    DissolveDurationSeconds = FMath::Max(0.1f, Settings->DissolveDurationSeconds);

    bReplicates = true;
    SetReplicateMovement(true);
    AIControllerClass = nullptr;
    AutoPossessAI = EAutoPossessAI::Disabled;
    bUseControllerRotationYaw = false;
    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    GetMesh()->SetRelativeLocation(FVector(0.0, 0.0, -88.0));
    GetMesh()->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetMesh()->SetCollisionObjectType(ECC_Pawn);
    GetMesh()->SetCollisionResponseToAllChannels(ECR_Block);
    GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    GetMesh()->SetCanEverAffectNavigation(false);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    VitalSet = CreateDefaultSubobject<URogueVitalAttributeSet>(TEXT("VitalSet"));
    AugmentSet = CreateDefaultSubobject<URogueAugmentAttributeSet>(TEXT("AugmentSet"));
    HealthComponent = CreateDefaultSubobject<URogueHealthComponent>(TEXT("HealthComponent"));
}

void ATrainingCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATrainingCharacter, DissolveStartServerSeconds);
}

UAbilitySystemComponent* ATrainingCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

float ATrainingCharacter::GetInitialTrainingHealth() const
{
    return FMath::Max(1.0f, GetDefault<UTrainingSettings>()->CombatHealth);
}

void ATrainingCharacter::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    if (HasAuthority())
    {
        const float InitialHealth = GetInitialTrainingHealth();
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetMaxHealthAttribute(), InitialHealth);
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetHealthAttribute(), InitialHealth);
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetMaxShieldAttribute(), 0.0f);
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetShieldAttribute(), 0.0f);
    }
    if (!GetMesh()->GetPhysicsAsset())
    {
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }
    HealthComponent->InitializeWithAbilitySystem(AbilitySystem);
    if (HasAuthority())
    {
        TrainingDamageTakenHandle = AbilitySystem->AddGameplayEventTagContainerDelegate(
            FGameplayTagContainer(RogueTags::Event_Combat_DamageTaken),
            FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleTrainingDamageTaken));
    }
    if (DissolveStartServerSeconds >= 0.0)
    {
        StartDissolvePresentation();
    }
}

bool ATrainingCharacter::IsAlive() const
{
    return DissolveStartServerSeconds < 0.0 && !HealthComponent->IsDead();
}

UCombatHitZoneDefinition* ATrainingCharacter::GetCombatHitZoneDefinition_Implementation() const
{
    return HitZoneDefinition;
}

bool ATrainingCharacter::ShouldSpawnHitscanImpactParticle_Implementation() const
{
    return bEnableHitscanImpactParticles;
}

void ATrainingCharacter::HandleDeath_Implementation(const FRogueDeathContext&)
{
    BeginDissolveRemoval();
}

void ATrainingCharacter::PlayDeathReaction_Implementation(const FRogueDeathReaction&)
{
    if (HasAuthority())
    {
        BeginDissolveRemoval();
    }
    else if (DissolveStartServerSeconds >= 0.0)
    {
        StartDissolvePresentation();
    }
    else
    {
        StopTrainingMotion();
    }
}

float ATrainingCharacter::GetDissolveDuration() const
{
    return FMath::Clamp(DissolveDurationSeconds, 0.1f, 10.0f);
}

void ATrainingCharacter::BeginDissolveRemoval()
{
    if (!HasAuthority() || DissolveStartServerSeconds >= 0.0)
    {
        return;
    }
    DissolveStartServerSeconds = GetWorld()->GetTimeSeconds();
    AbilitySystem->SetLooseGameplayTagCount(RogueTags::State_Combat_IsDead, 1);
    StartDissolvePresentation();
    SetLifeSpan(GetDissolveDuration());
    ForceNetUpdate();
}

void ATrainingCharacter::StopTrainingMotion()
{
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    SetActorEnableCollision(false);
    GetMesh()->bPauseAnims = true;
    GetMesh()->SetSimulatePhysics(false);
}

void ATrainingCharacter::OnRep_DissolveStartServerSeconds()
{
    if (HasActorBegunPlay() && DissolveStartServerSeconds >= 0.0)
    {
        StartDissolvePresentation();
    }
}

void ATrainingCharacter::StartDissolvePresentation()
{
    if (bDeathPresentationStarted || DissolveStartServerSeconds < 0.0)
    {
        return;
    }
    bDeathPresentationStarted = true;
    StopTrainingMotion();
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }
    if (!DissolveMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] %s: Dissolve Material is not assigned. Removal will still finish."),
            *GetName());
    }
    else
    {
        const AGameStateBase* WorldGameState = GetWorld()->GetGameState();
        const double ServerNow = WorldGameState ? WorldGameState->GetServerWorldTimeSeconds()
            : GetWorld()->GetTimeSeconds();
        const float AlreadyElapsed = static_cast<float>(FMath::Max(0.0, ServerNow - DissolveStartServerSeconds));
        const float LocalMaterialStart = static_cast<float>(GetWorld()->GetTimeSeconds()) - AlreadyElapsed;
        const FVector EffectOrigin = GetMesh()->Bounds.Origin - FVector(0.0, 0.0, GetMesh()->Bounds.BoxExtent.Z);
        const float EffectHeight = FMath::Max(1.0f, static_cast<float>(GetMesh()->Bounds.BoxExtent.Z * 2.0));
        DissolveInstance = UMaterialInstanceDynamic::Create(DissolveMaterial, this);
        if (DissolveInstance)
        {
            DissolveInstance->SetScalarParameterValue(TEXT("DissolveStartTime"), LocalMaterialStart);
            DissolveInstance->SetScalarParameterValue(TEXT("DissolveDuration"), GetDissolveDuration());
            DissolveInstance->SetScalarParameterValue(DissolveParameterName, DissolveStartValue);
            DissolveInstance->SetVectorParameterValue(TEXT("DissolveOriginWS"),
                FLinearColor(static_cast<float>(EffectOrigin.X), static_cast<float>(EffectOrigin.Y),
                    static_cast<float>(EffectOrigin.Z), 0.0f));
            DissolveInstance->SetScalarParameterValue(TEXT("DissolveHeight"), EffectHeight);
            for (int32 MaterialIndex = 0; MaterialIndex < GetMesh()->GetNumMaterials(); ++MaterialIndex)
            {
                GetMesh()->SetMaterial(MaterialIndex, DissolveInstance);
            }
        }
    }
    GetWorld()->GetTimerManager().SetTimer(DissolveUpdateTimer, this,
        &ThisClass::UpdateDissolvePresentation, 1.0f / 60.0f, true);
    UpdateDissolvePresentation();
}

void ATrainingCharacter::UpdateDissolvePresentation()
{
    const AGameStateBase* WorldGameState = GetWorld()->GetGameState();
    const double ServerNow = WorldGameState ? WorldGameState->GetServerWorldTimeSeconds()
        : GetWorld()->GetTimeSeconds();
    const float Elapsed = static_cast<float>(FMath::Max(0.0, ServerNow - DissolveStartServerSeconds));
    const float Alpha = FMath::Clamp(Elapsed / GetDissolveDuration(), 0.0f, 1.0f);
    const float Value = FMath::Lerp(DissolveStartValue, DissolveEndValue, Alpha);
    if (DissolveInstance)
    {
        DissolveInstance->SetScalarParameterValue(DissolveParameterName, Value);
    }
    if (Alpha >= 1.0f)
    {
        GetWorld()->GetTimerManager().ClearTimer(DissolveUpdateTimer);
        SetActorHiddenInGame(true);
    }
}

void ATrainingCharacter::HandleTrainingDamageTaken(FGameplayTag, const FGameplayEventData* Payload)
{
    if (!HasAuthority() || !Payload || bEliminationReported) { return; }
    const bool bKilled = HealthComponent->IsDead() || RemovesOnAnyAcceptedHit();
    if (bKilled)
    {
        bEliminationReported = true;
        BeginDissolveRemoval();
    }
    OnTrainingHit.Broadcast(*this, Payload->Instigator.Get(),
        FMath::Max(0.0f, Payload->EventMagnitude), bKilled);
}

void ATrainingCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorld()->GetTimerManager().ClearTimer(DissolveUpdateTimer);
    if (TrainingDamageTakenHandle.IsValid())
    {
        AbilitySystem->RemoveGameplayEventTagContainerDelegate(
            FGameplayTagContainer(RogueTags::Event_Combat_DamageTaken), TrainingDamageTakenHandle);
        TrainingDamageTakenHandle.Reset();
    }
    OnTrainingHit.Clear();
    DissolveInstance = nullptr;
    HealthComponent->UninitializeFromAbilitySystem();
    Super::EndPlay(EndPlayReason);
}
