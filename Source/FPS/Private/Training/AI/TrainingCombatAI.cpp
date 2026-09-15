#include "Training/AI/TrainingCombatAI.h"

#include "AbilitySystemComponent.h"
#include "Combat/CombatHitZoneDefinition.h"
#include "Combat/RogueDeathPresentation.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GAS/RogueAugmentAttributeSet.h"
#include "GAS/RogueHealthComponent.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Training/AI/TrainingCombatAIController.h"
#include "Training/TrainingArena.h"

ATrainingCombatAI::ATrainingCombatAI()
{
    bReplicates = true;
    SetReplicateMovement(true);
    AIControllerClass = ATrainingCombatAIController::StaticClass();
    // The scenario supplies arena/participant data before explicitly spawning the controller.
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
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0, 540.0, 0.0);
    GetCharacterMovement()->MaxWalkSpeed = 180.0f;
    GetCharacterMovement()->bUseRVOAvoidance = false;

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    VitalSet = CreateDefaultSubobject<URogueVitalAttributeSet>(TEXT("VitalSet"));
    AugmentSet = CreateDefaultSubobject<URogueAugmentAttributeSet>(TEXT("AugmentSet"));
    HealthComponent = CreateDefaultSubobject<URogueHealthComponent>(TEXT("HealthComponent"));
}

UAbilitySystemComponent* ATrainingCombatAI::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

void ATrainingCombatAI::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    if (HasAuthority())
    {
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetMaxHealthAttribute(), TrainingRules::CombatHealth);
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetHealthAttribute(), TrainingRules::CombatHealth);
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetMaxShieldAttribute(), 0.0f);
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetShieldAttribute(), 0.0f);
    }
    HealthComponent->InitializeWithAbilitySystem(AbilitySystem);
}

bool ATrainingCombatAI::HasConfiguredBehavior() const
{
    const ATrainingCombatAIController* DefaultAIController = AIControllerClass
        ? Cast<ATrainingCombatAIController>(AIControllerClass->GetDefaultObject()) : nullptr;
    return DefaultAIController && DefaultAIController->HasConfiguredStateTree();
}

bool ATrainingCombatAI::InitializeForTraining(ATrainingArena& InArena,
    APlayerController& InParticipant, const FTrainingDistanceBand& InBand)
{
    if (!HasAuthority() || !IsAlive())
    {
        return false;
    }
    TrainingArena = &InArena;
    Participant = &InParticipant;
    DistanceBand = InBand;
    SpawnDefaultController();
    ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetController());
    return TrainingController && TrainingController->StartTrainingBrain();
}

ATrainingArena* ATrainingCombatAI::GetTrainingArena() const
{
    return TrainingArena.Get();
}

APlayerController* ATrainingCombatAI::GetParticipant() const
{
    return Participant.Get();
}

bool ATrainingCombatAI::IsAlive() const
{
    return !HealthComponent->IsDead();
}

UCombatHitZoneDefinition* ATrainingCombatAI::GetCombatHitZoneDefinition_Implementation() const
{
    return HitZoneDefinition;
}

void ATrainingCombatAI::HandleDeath_Implementation(const FRogueDeathContext&)
{
    if (HasAuthority())
    {
        // HealthComponent has already applied the replicated death reaction.
        SetLifeSpan(TrainingRules::CorpseLifetimeSeconds);
        ForceNetUpdate();
    }
}

void ATrainingCombatAI::PlayDeathReaction_Implementation(const FRogueDeathReaction& Reaction)
{
    if (bDeathPresentationStarted)
    {
        return;
    }
    bDeathPresentationStarted = true;
    if (ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetController()))
    {
        TrainingController->StopTrainingBrain();
    }
    RogueDeathPresentation::StartRagdoll(*this, Reaction, FRogueRagdollSettings());
    StartDissolve();
}

void ATrainingCombatAI::StartDissolve()
{
    if (GetNetMode() == NM_DedicatedServer || !DissolveMaterial)
    {
        return;
    }
    // No visual Tick: all material slots get the configured dissolve material immediately.
    for (int32 MaterialIndex = 0; MaterialIndex < GetMesh()->GetNumMaterials(); ++MaterialIndex)
    {
        UMaterialInstanceDynamic* DissolveInstance = UMaterialInstanceDynamic::Create(DissolveMaterial, this);
        DissolveInstance->SetScalarParameterValue(TEXT("DissolveStartTime"), GetWorld()->GetTimeSeconds());
        DissolveInstance->SetScalarParameterValue(TEXT("DissolveDuration"), TrainingRules::CorpseLifetimeSeconds);
        GetMesh()->SetMaterial(MaterialIndex, DissolveInstance);
    }
}

void ATrainingCombatAI::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    HealthComponent->UninitializeFromAbilitySystem();
    if (HasAuthority())
    {
        if (ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetController()))
        {
            TrainingController->StopTrainingBrain();
            TrainingController->UnPossess();
            TrainingController->Destroy();
        }
    }
    Super::EndPlay(EndPlayReason);
}
