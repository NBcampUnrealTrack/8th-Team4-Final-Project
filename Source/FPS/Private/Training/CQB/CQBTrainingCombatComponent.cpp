#include "Training/CQB/CQBTrainingCombatComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GAS/Damage/RogueDamageSubsystem.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "Perception/AISense_Hearing.h"
#include "Tags/GameplayTags.h"
#include "Training/CQB/CQBTrainingAIComponent.h"
#include "Training/CQB/CQBTrainingAIController.h"
#include "Training/CQB/CQBTrainingAwarenessComponent.h"
#include "Training/CQB/CQBTrainingLocomotionComponent.h"

UCQBTrainingCombatComponent::UCQBTrainingCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCQBTrainingCombatComponent::Configure(
    UCQBTrainingAIComponent& InConfig,
    UCQBTrainingAwarenessComponent& InAwareness,
    UCQBTrainingLocomotionComponent& InLocomotion)
{
    ConfigRef = &InConfig;
    AwarenessRef = &InAwareness;
    LocomotionRef = &InLocomotion;
    ResetRuntimeState();
}

void UCQBTrainingCombatComponent::ResetRuntimeState()
{
    BurstShotsRemaining = 0;
    BurstShotIndex = 0;
    NextShotTime = 0.0;
    NextBurstTime = 0.0;
    HitPenaltyUntil = -1.0;
    bFiringLaneMovePrepared = false;
    FiringLaneSourceLocation = FVector::ZeroVector;
    NextFiringLanePrepareTime = 0.0;
    ResetTactics();
}

void UCQBTrainingCombatComponent::EnterReaction()
{
    if (UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion())
    {
        Locomotion->Stop();
    }
    BurstShotsRemaining = 0;
    BurstShotIndex = 0;
}

void UCQBTrainingCombatComponent::TickReaction(const float DeltaTime)
{
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    if (!Awareness || !Awareness->HasVisibleTarget())
    {
        return;
    }

    Awareness->RefreshVisibleTargetLocation();
    UpdateCombatFacing(DeltaTime);
}

void UCQBTrainingCombatComponent::EnterEngage()
{
    bEngagementActive = true;
    GetLocomotion()->Stop();
    BurstShotsRemaining = 0;
    BurstShotIndex = 0;
    ResetFiringLaneMove();
    bTacticalMovePrepared = false;
    PeekPhase = EPeekPhase::None;
    bCoverChosenWithSight = GetAwareness()->HasVisibleTarget();
    const double Now = GetWorld()->GetTimeSeconds();
    NextBurstTime = Now;
    NextShotTime = Now;
    NextTacticalEvaluationTime = 0.0;
    TacticalIntent = ECQBTacticalIntent::DirectFire;
    if (PositionStartTime <= 0.0)
    {
        PositionStartTime = Now;
    }
    EvaluateTacticalIntent();
}

void UCQBTrainingCombatComponent::TickEngage(const float DeltaTime)
{
    EvaluateTacticalIntent();
    TickTacticalIntent(DeltaTime, TacticalIntent);
}

void UCQBTrainingCombatComponent::TickFire(const double Now, const bool bMoving)
{
    const UCQBTrainingAIComponent* Config = GetConfig();
    const UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    const ACQBTrainingAIController* Controller = GetController();
    if (!Awareness->HasVisibleTarget() || !Awareness->IsReactionReady(Now)
        || !Controller->IsCombatTargetValid(Awareness->GetTargetActor()))
    {
        BurstShotsRemaining = 0;
        return;
    }

    const FVector ShotStart = ResolveShotStart();
    const FVector AimPoint = ResolveTargetAimPoint();
    const float Distance = FVector::Dist2D(Config->GetActorLocation(), Awareness->GetThreatLocation());
    if (!HasClearShotToTarget(ShotStart, AimPoint)
        || !GetLocomotion()->IsFacing(AimPoint, Config->ReactionSettings.FireFacingToleranceDegrees)
        || (bMoving && Distance > 700.0f))
    {
        BurstShotsRemaining = 0;
        return;
    }

    const FCQBCombatSettings& Combat = Config->CombatSettings;
    if (BurstShotsRemaining <= 0)
    {
        if (Now < NextBurstTime)
        {
            return;
        }
        int32 MinShots = FMath::Max(1, FMath::Min(Combat.BurstMinShots, Combat.BurstMaxShots));
        int32 MaxShots = FMath::Max(MinShots, FMath::Max(Combat.BurstMinShots, Combat.BurstMaxShots));
        if (TacticalIntent == ECQBTacticalIntent::PeekFire)
        {
            MinShots = 1;
            MaxShots = FMath::Min(3, MaxShots);
        }
        else if (TacticalIntent == ECQBTacticalIntent::HoldAngle || bMoving)
        {
            MinShots = 1;
            MaxShots = FMath::Min(2, MaxShots);
        }
        BurstShotsRemaining = FMath::RandRange(MinShots, MaxShots);
        BurstShotIndex = 0;
        NextShotTime = Now;
    }
    if (Now < NextShotTime)
    {
        return;
    }

    FireOneShot(ShotStart, AimPoint);
    --BurstShotsRemaining;
    ++BurstShotIndex;
    const float CloseRangeScale = Distance < 500.0f ? 0.8f : 1.0f;
    if (BurstShotsRemaining > 0)
    {
        NextShotTime = Now + CloseRangeScale * FMath::FRandRange(
            FMath::Min(Combat.ShotIntervalMinSeconds, Combat.ShotIntervalMaxSeconds),
            FMath::Max(Combat.ShotIntervalMinSeconds, Combat.ShotIntervalMaxSeconds));
    }
    else
    {
        ++BurstsFromPosition;
        const float IntentScale = TacticalIntent == ECQBTacticalIntent::HoldAngle ? 1.25f : 1.0f;
        NextBurstTime = Now + CloseRangeScale * IntentScale * FMath::FRandRange(
            FMath::Min(Combat.BurstCooldownMinSeconds, Combat.BurstCooldownMaxSeconds),
            FMath::Max(Combat.BurstCooldownMinSeconds, Combat.BurstCooldownMaxSeconds));
        if (TacticalIntent == ECQBTacticalIntent::PeekFire && PeekPhase == EPeekPhase::Exposed)
        {
            BeginPeekReturn(Now);
        }
    }
}

void UCQBTrainingCombatComponent::ExitCombat()
{
    FinishDefensiveEpisode();
    bEngagementActive = false;
    TacticalIntent = ECQBTacticalIntent::DirectFire;
    bTacticalMovePrepared = false;
    PeekPhase = EPeekPhase::None;
    BurstShotsRemaining = 0;
    BurstShotIndex = 0;
    ResetFiringLaneMove();
    if (UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion())
    {
        Locomotion->Stop();
    }
}

void UCQBTrainingCombatComponent::NotifyPawnHit()
{
    const UCQBTrainingAIComponent* Config = GetConfig();
    UWorld* World = GetWorld();
    if (!Config || !World)
    {
        return;
    }

    NextTacticalEvaluationTime = 0.0;
    HitPenaltyUntil = World->GetTimeSeconds()
        + FMath::Max(0.0f, Config->AccuracySettings.HitReactionPenaltySeconds);
}

ACQBTrainingAIController* UCQBTrainingCombatComponent::GetController() const
{
    return Cast<ACQBTrainingAIController>(GetOwner());
}

UCQBTrainingAIComponent* UCQBTrainingCombatComponent::GetConfig() const
{
    return ConfigRef.Get();
}

UCQBTrainingAwarenessComponent* UCQBTrainingCombatComponent::GetAwareness() const
{
    return AwarenessRef.Get();
}

UCQBTrainingLocomotionComponent* UCQBTrainingCombatComponent::GetLocomotion() const
{
    return LocomotionRef.Get();
}

FVector UCQBTrainingCombatComponent::ResolveShotStart() const
{
    const UCQBTrainingAIComponent* Config = GetConfig();
    if (!Config)
    {
        return FVector::ZeroVector;
    }

    FTransform MuzzleTransform;
    return Config->ResolveMuzzleTransform(MuzzleTransform)
        ? MuzzleTransform.GetLocation()
        : Config->GetPawnViewLocation();
}

FVector UCQBTrainingCombatComponent::ResolveTargetAimPoint() const
{
    return GetAwareness()->GetThreatLocation()
        + FVector(0.0f, 0.0f, GetConfig()->AccuracySettings.AimPointHeightOffset);
}

float UCQBTrainingCombatComponent::ComputeAimErrorDegrees() const
{
    const UCQBTrainingAIComponent* Config = GetConfig();
    const UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    const AActor* Target = Awareness ? Awareness->GetTargetActor() : nullptr;
    const UWorld* World = GetWorld();
    if (!Config || !Awareness || !Target || !World)
    {
        return 0.0f;
    }

    const FCQBAccuracySettings& Accuracy = Config->AccuracySettings;
    const double Now = World->GetTimeSeconds();
    const float VisibleSeconds = static_cast<float>(
        FMath::Max(0.0, Now - Awareness->GetSightAcquiredTime()));
    const float SettleAlpha = FMath::Clamp(
        VisibleSeconds / FMath::Max(0.01f, Accuracy.AimSettleSeconds), 0.0f, 1.0f);

    float Error = FMath::Lerp(
        FMath::Max(0.0f, Accuracy.InitialAimErrorDegrees),
        FMath::Max(0.0f, Accuracy.SettledAimErrorDegrees),
        SettleAlpha);

    if (Config->GetVelocity().SizeSquared2D() > FMath::Square(20.0f))
    {
        Error += FMath::Max(0.0f, Accuracy.MovingPenaltyDegrees);
    }
    if (Now < HitPenaltyUntil)
    {
        Error += FMath::Max(0.0f, Accuracy.HitReactionPenaltyDegrees);
    }

    const float DistanceMeters = FVector::Distance(
        Config->GetActorLocation(), Awareness->GetThreatLocation()) / 100.0f;
    const float ExtraTens = FMath::Max(0.0f, DistanceMeters - 5.0f) / 10.0f;
    Error += ExtraTens * FMath::Max(0.0f, Accuracy.DistancePenaltyPer10Meters);
    Error += static_cast<float>(BurstShotIndex)
        * FMath::Max(0.0f, Accuracy.BurstRecoilDegreesPerShot);
    if (TacticalIntent == ECQBTacticalIntent::HoldAngle)
    {
        Error *= 0.85f;
    }
    return FMath::Max(0.0f, Error);
}

bool UCQBTrainingCombatComponent::HasClearShotToTarget(const FVector& Start, const FVector& AimPoint) const
{
    const ACQBTrainingAIController* Controller = GetController();
    const UCQBTrainingAIComponent* Config = GetConfig();
    const UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UWorld* World = GetWorld();
    const AActor* Target = Awareness ? Awareness->GetTargetActor() : nullptr;
    if (!Controller || !Config || !World || !Controller->IsCombatTargetValid(Target))
    {
        return false;
    }

    if (FVector::DistSquared(Start, AimPoint)
        > FMath::Square(FMath::Max(100.0f, Config->CombatSettings.MaxFireRange)))
    {
        return false;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBTrainingLineOfFire), true, Controller->GetPawn());
    Params.AddIgnoredActor(Controller);
    FHitResult Hit;
    return World->LineTraceSingleByChannel(Hit, Start, AimPoint, ECC_Visibility, Params)
        && Hit.GetActor() == Target;
}

bool UCQBTrainingCombatComponent::TickAcquireFiringLane(
    const float DeltaTime,
    const double NowSeconds)
{
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    AActor* Target = Awareness ? Awareness->GetTargetActor() : nullptr;
    if (!Config || !Awareness || !Locomotion || !Target)
    {
        return false;
    }

    const FVector KnownLocation = Awareness->GetLastKnownTargetLocation();
    const bool bGoalChanged = !bFiringLaneMovePrepared
        || FVector::DistSquared2D(KnownLocation, FiringLaneSourceLocation) > FMath::Square(150.0f);
    if (bGoalChanged && NowSeconds >= NextFiringLanePrepareTime)
    {
        Locomotion->Stop();
        Locomotion->SetMoveSpeed(Config->CombatSettings.RepositionSpeed);
        bFiringLaneMovePrepared = Locomotion->PrepareMoveGoal(
            KnownLocation, Config->CombatSettings.RepositionAcceptanceRadius);
        FiringLaneSourceLocation = KnownLocation;
        NextFiringLanePrepareTime = NowSeconds
            + FMath::Max(0.05f, Config->PatrolSettings.MoveRetryDelaySeconds);
        if (bFiringLaneMovePrepared)
        {
            Locomotion->RequestPreparedMove();
        }
    }

    if (bFiringLaneMovePrepared)
    {
        Locomotion->TickPreparedMove(DeltaTime, 0.0f);
    }

    return bFiringLaneMovePrepared;
}

void UCQBTrainingCombatComponent::ResetFiringLaneMove()
{
    bFiringLaneMovePrepared = false;
    FiringLaneSourceLocation = FVector::ZeroVector;
    NextFiringLanePrepareTime = 0.0;
}

void UCQBTrainingCombatComponent::UpdateCombatFacing(const float DeltaTime)
{
    const UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    if (!Config || !Locomotion)
    {
        return;
    }

    Locomotion->TurnToward(
        ResolveTargetAimPoint(),
        DeltaTime,
        Config->ReactionSettings.CombatTurnSpeedDegreesPerSecond,
        true);
}

void UCQBTrainingCombatComponent::FireOneShot(const FVector& Start, const FVector& AimPoint)
{
    ACQBTrainingAIController* Controller = GetController();
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UWorld* World = GetWorld();
    AActor* Target = Awareness ? Awareness->GetTargetActor() : nullptr;
    if (!Controller || !Config || !Awareness || !World
        || !Controller->IsCombatTargetValid(Target)
        || !Awareness->HasVisibleTarget())
    {
        return;
    }

    const FVector BaseDirection = (AimPoint - Start).GetSafeNormal();
    if (BaseDirection.IsNearlyZero())
    {
        return;
    }

    const float ErrorRadians = FMath::DegreesToRadians(ComputeAimErrorDegrees());
    const FVector ShotDirection = ErrorRadians > KINDA_SMALL_NUMBER
        ? FMath::VRandCone(BaseDirection, ErrorRadians)
        : BaseDirection;
    const FVector IntendedEnd = Start
        + ShotDirection * FMath::Max(100.0f, Config->CombatSettings.MaxFireRange);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBTrainingShot), true, Controller->GetPawn());
    Params.AddIgnoredActor(Controller);
    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(
        Hit, Start, IntendedEnd, ECC_Visibility, Params);
    const FVector End = bHit ? Hit.ImpactPoint : IntendedEnd;

    if (bHit)
    {
        ApplyShotDamage(Hit);
    }

    Config->PlayShotPresentation(Start, End, bHit);
    UAISense_Hearing::ReportNoiseEvent(
        this,
        Start,
        FMath::Max(0.0f, Config->CombatSettings.GunshotNoiseLoudness),
        Controller->GetPawn(),
        FMath::Max(0.0f, Config->CombatSettings.GunshotNoiseMaxRange),
        TEXT("CQB_AI_Gunshot"));

}

void UCQBTrainingCombatComponent::ApplyShotDamage(const FHitResult& HitResult)
{
    ACQBTrainingAIController* Controller = GetController();
    UCQBTrainingAIComponent* Config = GetConfig();
    UWorld* World = GetWorld();
    AActor* HitActor = HitResult.GetActor();
    if (!Controller || !Config || !World || !Controller->IsCombatTargetValid(HitActor))
    {
        return;
    }

    if (URogueDamageSubsystem* DamageSubsystem = World->GetSubsystem<URogueDamageSubsystem>())
    {
        FRogueDamageParams DamageParams;
        DamageParams.Instigator = Controller->GetPawn();
        DamageParams.Causer = Controller->GetPawn();
        DamageParams.SourceTag = RogueTags::Damage_Source_Weapon;
        DamageSubsystem->ApplyFlatDamage(
            DamageParams, HitActor, FMath::Max(1.0f, Config->CombatSettings.DamagePerShot));
    }
}
