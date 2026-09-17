#include "Training/TrainingPopUpTarget.h"

#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GAS/RogueAugmentAttributeSet.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GameplayTags.h"
#include "TimerManager.h"
#include "Training/TrainingPlayerComponent.h"
#include "Training/TrainingSettings.h"

namespace
{
APlayerController* ResolveDamageInstigatorController(const UObject* InstigatorObject)
{
    const AActor* InstigatorActor = Cast<AActor>(InstigatorObject);
    if (!IsValid(InstigatorActor))
    {
        return nullptr;
    }

    if (const APlayerController* PlayerController = Cast<APlayerController>(InstigatorActor))
    {
        return const_cast<APlayerController*>(PlayerController);
    }

    if (const APawn* Pawn = Cast<APawn>(InstigatorActor))
    {
        return Cast<APlayerController>(Pawn->GetController());
    }

    return Cast<APlayerController>(InstigatorActor->GetOwner());
}
}

ATrainingPopUpTarget::ATrainingPopUpTarget()
{
    TargetMaxHealth = FMath::Max(1.0f, GetDefault<UTrainingSettings>()->CombatHealth);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    BaseMesh->SetupAttachment(SceneRoot);
    BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BaseMesh->SetCanEverAffectNavigation(false);

    TargetPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TargetPivot"));
    TargetPivot->SetupAttachment(SceneRoot);
    TargetPivot->SetMobility(EComponentMobility::Movable);

    TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
    TargetMesh->SetupAttachment(TargetPivot);
    TargetMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
    TargetMesh->SetMobility(EComponentMobility::Movable);
    TargetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TargetMesh->SetCanEverAffectNavigation(false);

    HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
    HitBox->SetupAttachment(TargetPivot);
    HitBox->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
    HitBox->SetMobility(EComponentMobility::Movable);
    HitBox->SetBoxExtent(FVector(8.0f, 45.0f, 90.0f));
    HitBox->SetGenerateOverlapEvents(false);
    HitBox->SetCanEverAffectNavigation(false);
    ConfigureHitBoxCollision();

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    VitalSet = CreateDefaultSubobject<URogueVitalAttributeSet>(TEXT("VitalSet"));
    AugmentSet = CreateDefaultSubobject<URogueAugmentAttributeSet>(TEXT("AugmentSet"));
}

void ATrainingPopUpTarget::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    ConfigureTargetComponents();
    ConfigureHitBoxCollision();
}

void ATrainingPopUpTarget::BeginPlay()
{
    Super::BeginPlay();

    ConfigureTargetComponents();
    ConfigureHitBoxCollision();

    AbilitySystem->InitAbilityActorInfo(this, this);
    if (HasAuthority())
    {
        InitializeHealth();
        DamageTakenHandle = AbilitySystem->AddGameplayEventTagContainerDelegate(
            FGameplayTagContainer(RogueTags::Event_Combat_DamageTaken),
            FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(
                this, &ThisClass::HandleDamageTaken));
        OutOfHealthHandle = VitalSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
    }

    RaisedRotation = TargetPivot ? TargetPivot->GetRelativeRotation() : FRotator::ZeroRotator;
    MotionAlpha = TargetState == ETrainingPopUpTargetState::Down ? 1.0f : 0.0f;
    UpdateTargetRotation();
    OnRep_TargetState();
}


void ATrainingPopUpTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(ResetTimer);

    if (DamageTakenHandle.IsValid() && AbilitySystem)
    {
        AbilitySystem->RemoveGameplayEventTagContainerDelegate(
            FGameplayTagContainer(RogueTags::Event_Combat_DamageTaken), DamageTakenHandle);
        DamageTakenHandle.Reset();
    }
    if (OutOfHealthHandle.IsValid() && VitalSet)
    {
        VitalSet->OnOutOfHealth.Remove(OutOfHealthHandle);
        OutOfHealthHandle.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void ATrainingPopUpTarget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATrainingPopUpTarget, TargetState);
}

UAbilitySystemComponent* ATrainingPopUpTarget::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

float ATrainingPopUpTarget::GetCurrentHealth() const
{
    return VitalSet ? VitalSet->GetHealth() : 0.0f;
}

float ATrainingPopUpTarget::GetMaxHealth() const
{
    return VitalSet ? VitalSet->GetMaxHealth() : FMath::Max(1.0f, TargetMaxHealth);
}

void ATrainingPopUpTarget::InitializeHealth()
{
    if (!HasAuthority() || !AbilitySystem || !VitalSet)
    {
        return;
    }

    VitalSet->ClearDeathDeclared();
    AbilitySystem->SetLooseGameplayTagCount(RogueTags::State_Combat_IsDead, 0);

    const float Health = FMath::Max(1.0f, TargetMaxHealth);
    AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetMaxHealthAttribute(), Health);
    AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetHealthAttribute(), Health);
    AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetMaxShieldAttribute(), 0.0f);
    AbilitySystem->SetNumericAttributeBase(URogueVitalAttributeSet::GetShieldAttribute(), 0.0f);
}

void ATrainingPopUpTarget::ResetHealthForNextCycle()
{
    InitializeHealth();
}

void ATrainingPopUpTarget::HandleOutOfHealth(UAbilitySystemComponent* VictimASC,
    const FGameplayEffectContextHandle&)
{
    if (!HasAuthority() || VictimASC != AbilitySystem)
    {
        return;
    }

    AbilitySystem->SetLooseGameplayTagCount(RogueTags::State_Combat_IsDead, 1);
    KnockDown();
}

void ATrainingPopUpTarget::HandleDamageTaken(FGameplayTag, const FGameplayEventData* Payload)
{
    if (!HasAuthority() || !Payload)
    {
        return;
    }

    const float Damage = FMath::Max(0.0f, Payload->EventMagnitude);
    if (Damage <= 0.0f)
    {
        return;
    }

    const bool bKilled = VitalSet && VitalSet->IsDeathDeclared();
    if (APlayerController* PlayerController = ResolveDamageInstigatorController(Payload->Instigator.Get()))
    {
        if (UTrainingPlayerComponent* TrainingUI =
            PlayerController->FindComponentByClass<UTrainingPlayerComponent>())
        {
            TrainingUI->NotifyTrainingDamage(Damage, bKilled, GetActorLocation());
        }
    }
}

void ATrainingPopUpTarget::HandleHitscanImpact(AActor*, const FVector& ImpactPoint, const FVector&)
{
    if (!HasAuthority() || TargetState != ETrainingPopUpTargetState::Up)
    {
        return;
    }

    MulticastPlayHitSound(ImpactPoint);
}

void ATrainingPopUpTarget::MulticastPlayHitSound_Implementation(const FVector_NetQuantize ImpactPoint)
{
    if (!HitSound || GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    UGameplayStatics::PlaySoundAtLocation(
        this,
        HitSound,
        FVector(ImpactPoint),
        HitSoundVolume,
        HitSoundPitch);
}

void ATrainingPopUpTarget::KnockDown()
{
    if (!HasAuthority() || TargetState != ETrainingPopUpTargetState::Up)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(ResetTimer);
    SetTargetState(ETrainingPopUpTargetState::Falling);
}

void ATrainingPopUpTarget::ResetTarget()
{
    if (!HasAuthority())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(ResetTimer);
    ResetHealthForNextCycle();

    if (TargetState == ETrainingPopUpTargetState::Up)
    {
        MotionAlpha = 0.0f;
        UpdateTargetRotation();
        SetHitBoxActive(true);
        return;
    }

    SetTargetState(ETrainingPopUpTargetState::Rising);
}

void ATrainingPopUpTarget::BeginRise()
{
    if (HasAuthority())
    {
        ResetHealthForNextCycle();
        SetTargetState(ETrainingPopUpTargetState::Rising);
    }
}

void ATrainingPopUpTarget::SetTargetState(const ETrainingPopUpTargetState NewState)
{
    if (!HasAuthority() || TargetState == NewState)
    {
        return;
    }

    TargetState = NewState;
    OnRep_TargetState();
    ForceNetUpdate();
}

void ATrainingPopUpTarget::OnRep_TargetState()
{
    switch (TargetState)
    {
    case ETrainingPopUpTargetState::Up:
        MotionAlpha = 0.0f;
        UpdateTargetRotation();
        SetHitBoxActive(true);
        SetActorTickEnabled(false);
        break;

    case ETrainingPopUpTargetState::Falling:
        SetHitBoxActive(false);
        SetActorTickEnabled(true);
        break;

    case ETrainingPopUpTargetState::Down:
        MotionAlpha = 1.0f;
        UpdateTargetRotation();
        SetHitBoxActive(false);
        SetActorTickEnabled(false);
        break;

    case ETrainingPopUpTargetState::Rising:
        SetHitBoxActive(false);
        SetActorTickEnabled(true);
        break;
    }
}

void ATrainingPopUpTarget::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (TargetState == ETrainingPopUpTargetState::Falling)
    {
        const float Duration = FMath::Max(0.01f, FallDurationSeconds);
        MotionAlpha = FMath::Min(1.0f, MotionAlpha + DeltaSeconds / Duration);
        UpdateTargetRotation();

        if (HasAuthority() && MotionAlpha >= 1.0f - KINDA_SMALL_NUMBER)
        {
            SetTargetState(ETrainingPopUpTargetState::Down);
            if (DownHoldSeconds <= 0.0f)
            {
                BeginRise();
            }
            else
            {
                GetWorldTimerManager().SetTimer(ResetTimer, this, &ThisClass::BeginRise,
                    DownHoldSeconds, false);
            }
        }
        return;
    }

    if (TargetState == ETrainingPopUpTargetState::Rising)
    {
        const float Duration = FMath::Max(0.01f, RiseDurationSeconds);
        MotionAlpha = FMath::Max(0.0f, MotionAlpha - DeltaSeconds / Duration);
        UpdateTargetRotation();

        if (HasAuthority() && MotionAlpha <= KINDA_SMALL_NUMBER)
        {
            SetTargetState(ETrainingPopUpTargetState::Up);
        }
    }
}

void ATrainingPopUpTarget::UpdateTargetRotation()
{
    if (!TargetPivot)
    {
        return;
    }

    const float SmoothedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f,
        FMath::Clamp(MotionAlpha, 0.0f, 1.0f), 2.0f);
    const FQuat RaisedQuat = RaisedRotation.Quaternion();
    const FQuat DownQuat = (RaisedRotation + DownRotation).Quaternion();
    TargetPivot->SetRelativeRotation(FQuat::Slerp(RaisedQuat, DownQuat, SmoothedAlpha));
}

void ATrainingPopUpTarget::ConfigureTargetComponents()
{
    if (!SceneRoot || !TargetPivot || !TargetMesh || !HitBox)
    {
        return;
    }

    // Existing Blueprint children can preserve an older inherited attachment hierarchy.
    // Reassert the moving stack after Blueprint component templates have been applied.
    TargetPivot->SetMobility(EComponentMobility::Movable);
    TargetMesh->SetMobility(EComponentMobility::Movable);
    HitBox->SetMobility(EComponentMobility::Movable);

    if (TargetPivot->GetAttachParent() != SceneRoot)
    {
        TargetPivot->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepWorldTransform);
    }
    if (TargetMesh->GetAttachParent() != TargetPivot)
    {
        TargetMesh->AttachToComponent(TargetPivot, FAttachmentTransformRules::KeepWorldTransform);
    }
    if (HitBox->GetAttachParent() != TargetPivot)
    {
        HitBox->AttachToComponent(TargetPivot, FAttachmentTransformRules::KeepWorldTransform);
    }

    TargetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATrainingPopUpTarget::ConfigureHitBoxCollision()
{
    if (!HitBox)
    {
        return;
    }

    HitBox->SetCollisionObjectType(ECC_WorldDynamic);
    HitBox->SetCollisionResponseToAllChannels(ECR_Block);
    HitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    HitBox->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    HitBox->SetCollisionEnabled(
        TargetState == ETrainingPopUpTargetState::Up
            ? ECollisionEnabled::QueryOnly
            : ECollisionEnabled::NoCollision);
}

void ATrainingPopUpTarget::SetHitBoxActive(const bool bActive)
{
    if (!HitBox)
    {
        return;
    }

    if (bActive)
    {
        ConfigureHitBoxCollision();
        return;
    }

    HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
