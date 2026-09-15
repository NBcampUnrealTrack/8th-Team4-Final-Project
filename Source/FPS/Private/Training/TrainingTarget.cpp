#include "Training/TrainingTarget.h"

#include "AbilitySystemComponent.h"
#include "Combat/CombatHitZoneDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GAS/RogueAugmentAttributeSet.h"
#include "GAS/RogueHealthComponent.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Tags/GameplayTags.h"

ATrainingTarget::ATrainingTarget()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    HitCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("HitCollision"));
    SetRootComponent(HitCollision);
    HitCollision->SetBoxExtent(FVector(12.0, 35.0, 70.0));
    HitCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    HitCollision->SetCollisionObjectType(ECC_WorldDynamic);
    HitCollision->SetCollisionResponseToAllChannels(ECR_Block);
    HitCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    HitCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    HitCollision->SetCanEverAffectNavigation(false);
    DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    DisplayMesh->SetupAttachment(HitCollision);
    DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisplayMesh->SetCanEverAffectNavigation(false);

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    VitalSet = CreateDefaultSubobject<URogueVitalAttributeSet>(TEXT("VitalSet"));
    AugmentSet = CreateDefaultSubobject<URogueAugmentAttributeSet>(TEXT("AugmentSet"));
    HealthComponent = CreateDefaultSubobject<URogueHealthComponent>(TEXT("HealthComponent"));
}

UAbilitySystemComponent* ATrainingTarget::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

void ATrainingTarget::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    if (HasAuthority())
    {
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetMaxHealthAttribute(), 1.0f);
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetHealthAttribute(), 1.0f);
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetMaxShieldAttribute(), 0.0f);
        AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetShieldAttribute(), 0.0f);
        // Reuse the existing per-target damage event, including an applied hit rounded down to zero damage.
        DamageTakenHandle = AbilitySystem->AddGameplayEventTagContainerDelegate(
            FGameplayTagContainer(RogueTags::Event_Combat_DamageTaken),
            FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleDamageTaken));
    }
    HealthComponent->InitializeWithAbilitySystem(AbilitySystem);
}

void ATrainingTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (DamageTakenHandle.IsValid())
    {
        AbilitySystem->RemoveGameplayEventTagContainerDelegate(
            FGameplayTagContainer(RogueTags::Event_Combat_DamageTaken), DamageTakenHandle);
        DamageTakenHandle.Reset();
    }
    HealthComponent->UninitializeFromAbilitySystem();
    Super::EndPlay(EndPlayReason);
}

float ATrainingTarget::GetGroundOffsetCentimeters() const
{
    return static_cast<float>(HitCollision->GetScaledBoxExtent().Z) + 2.0f;
}

UCombatHitZoneDefinition* ATrainingTarget::GetCombatHitZoneDefinition_Implementation() const
{
    return HitZoneDefinition;
}

void ATrainingTarget::HandleDamageTaken(FGameplayTag, const FGameplayEventData*)
{
    FinishHit();
}

void ATrainingTarget::HandleDeath_Implementation(const FRogueDeathContext&)
{
    FinishHit();
}

void ATrainingTarget::PlayDeathReaction_Implementation(const FRogueDeathReaction&)
{
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
}

void ATrainingTarget::FinishHit()
{
    if (!HasAuthority() || bHitProcessed)
    {
        return;
    }
    bHitProcessed = true;
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
    // Leave the ASC alive until the current damage execution/event dispatch has returned.
    SetLifeSpan(0.01f);
    ForceNetUpdate();
}
