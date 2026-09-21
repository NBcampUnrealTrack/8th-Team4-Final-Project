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
    TargetMaxHealth = FMath::Max(1.0f, GetDefault<UTrainingSettings>()->PopUpTargetHealth);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = false;
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
    AbilitySystem->SetIsReplicated(false);
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
    InitializeHealth();
    DamageTakenHandle = AbilitySystem->AddGameplayEventTagContainerDelegate(
        FGameplayTagContainer(RogueTags::Event_Combat_DamageTaken),
        FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(
            this, &ThisClass::HandleDamageTaken));
    OutOfHealthHandle = VitalSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);

    RaisedRotation = TargetPivot ? TargetPivot->GetRelativeRotation() : FRotator::ZeroRotator;
    MotionAlpha = TargetState == ETrainingPopUpTargetState::Down ? 1.0f : 0.0f;
    UpdateTargetRotation();
    ApplyTargetState();
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
    if (!AbilitySystem || !VitalSet)
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
    if (VictimASC != AbilitySystem)
    {
        return;
    }

    AbilitySystem->SetLooseGameplayTagCount(RogueTags::State_Combat_IsDead, 1);
    KnockDown();
}

void ATrainingPopUpTarget::HandleDamageTaken(FGameplayTag, const FGameplayEventData* Payload)
{
    if (!Payload)
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

void ATrainingPopUpTarget::HandleHitscanImpact(AActor*, const FVector& ImpactPoint,
    const FVector& ShotDirection)
{
    if (TargetState != ETrainingPopUpTargetState::Up)
    {
        return;
    }

    CacheFallDirectionFromShot(ShotDirection);
    PlayHitSound(ImpactPoint);
}

void ATrainingPopUpTarget::PlayHitSound(const FVector& ImpactPoint)
{
    if (!HitSound)
    {
        return;
    }

    UGameplayStatics::PlaySoundAtLocation(
        this,
        HitSound,
        ImpactPoint,
        HitSoundVolume,
        HitSoundPitch);
}

void ATrainingPopUpTarget::KnockDown()
{
    if (TargetState != ETrainingPopUpTargetState::Up)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(ResetTimer);
    CommitPendingFallDirection();
    SetTargetState(ETrainingPopUpTargetState::Falling);
}

void ATrainingPopUpTarget::ResetTarget()
{
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
    ResetHealthForNextCycle();
    SetTargetState(ETrainingPopUpTargetState::Rising);
}

void ATrainingPopUpTarget::SetTargetState(const ETrainingPopUpTargetState NewState)
{
    if (TargetState == NewState)
    {
        return;
    }

    TargetState = NewState;
    ApplyTargetState();

    if (TargetState == ETrainingPopUpTargetState::Up)
    {
        PendingFallDirectionSign = 1;
    }
}

void ATrainingPopUpTarget::ApplyTargetState()
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

        if (MotionAlpha >= 1.0f - KINDA_SMALL_NUMBER)
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

        if (MotionAlpha <= KINDA_SMALL_NUMBER)
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
    const FQuat DownQuat = (RaisedRotation + GetDirectionalDownRotation()).Quaternion();
    TargetPivot->SetRelativeRotation(FQuat::Slerp(RaisedQuat, DownQuat, SmoothedAlpha));
}

void ATrainingPopUpTarget::CacheFallDirectionFromShot(const FVector& ShotDirection)
{
    const int8 ResolvedSign = ResolveFallDirectionSign(ShotDirection);
    if (ResolvedSign != 0)
    {
        PendingFallDirectionSign = ResolvedSign;
    }
}

void ATrainingPopUpTarget::CommitPendingFallDirection()
{
    const int8 NewSign = PendingFallDirectionSign < 0 ? -1 : 1;
    if (FallDirectionSign == NewSign)
    {
        return;
    }

    FallDirectionSign = NewSign;
    UpdateTargetRotation();
}

int8 ATrainingPopUpTarget::ResolveFallDirectionSign(const FVector& ShotDirection) const
{
    if (!SceneRoot || ShotDirection.IsNearlyZero())
    {
        return 0;
    }

    const FQuat RaisedQuat = RaisedRotation.Quaternion();
    const FQuat CanonicalDownQuat = (RaisedRotation + DownRotation).Quaternion();
    const FVector RaisedUp = RaisedQuat.RotateVector(FVector::UpVector).GetSafeNormal();

    FVector CanonicalFallDirection = CanonicalDownQuat.RotateVector(FVector::UpVector);
    CanonicalFallDirection = FVector::VectorPlaneProject(CanonicalFallDirection, RaisedUp).GetSafeNormal();
    if (CanonicalFallDirection.IsNearlyZero())
    {
        return 0;
    }

    FVector LocalShotDirection = SceneRoot->GetComponentTransform().InverseTransformVectorNoScale(
        ShotDirection.GetSafeNormal());
    LocalShotDirection = FVector::VectorPlaneProject(LocalShotDirection, RaisedUp).GetSafeNormal();
    if (LocalShotDirection.IsNearlyZero())
    {
        return 0;
    }

    const float Alignment = FVector::DotProduct(LocalShotDirection, CanonicalFallDirection);
    if (FMath::Abs(Alignment) <= FMath::Clamp(FallDirectionDeadZone, 0.0f, 1.0f))
    {
        return 0;
    }

    return Alignment >= 0.0f ? 1 : -1;
}

FRotator ATrainingPopUpTarget::GetDirectionalDownRotation() const
{
    return FallDirectionSign < 0
        ? FRotator(-DownRotation.Pitch, -DownRotation.Yaw, -DownRotation.Roll)
        : DownRotation;
}

void ATrainingPopUpTarget::ConfigureTargetComponents()
{
    if (!SceneRoot || !TargetPivot || !TargetMesh || !HitBox)
    {
        return;
    }

    // 기존 BP의 부착 계층을 보정해 메시와 히트박스를 함께 회전시킨다.
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
