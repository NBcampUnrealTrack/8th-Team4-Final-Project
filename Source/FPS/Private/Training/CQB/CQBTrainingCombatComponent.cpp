#include "Training/CQB/CQBTrainingCombatComponent.h"

#include "DrawDebugHelpers.h"
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
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    UWorld* World = GetWorld();
    if (Locomotion)
    {
        Locomotion->Stop();
    }

    BurstShotsRemaining = 0;
    BurstShotIndex = 0;
    ResetFiringLaneMove();
    NextBurstTime = World ? World->GetTimeSeconds() : 0.0;
    NextShotTime = NextBurstTime;
}

void UCQBTrainingCombatComponent::TickEngage(const float DeltaTime)
{
    ACQBTrainingAIController* Controller = GetController();
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    UWorld* World = GetWorld();
    AActor* Target = Awareness ? Awareness->GetTargetActor() : nullptr;
    if (!Controller || !Config || !Awareness || !Locomotion || !World
        || !Controller->IsCombatTargetValid(Target))
    {
        BurstShotsRemaining = 0;
        ResetFiringLaneMove();
        return;
    }

    const double Now = World->GetTimeSeconds();
    const bool bVisibleTarget = Awareness->HasVisibleTarget();
    const bool bDamageThreat = Awareness->HasDamageThreat(Now);
    if (!bVisibleTarget && !bDamageThreat)
    {
        BurstShotsRemaining = 0;
        ResetFiringLaneMove();
        return;
    }

    if (bVisibleTarget)
    {
        Awareness->RefreshVisibleTargetLocation();
    }
    UpdateCombatFacing(DeltaTime);

    const FVector ShotStart = ResolveShotStart();
    const FVector AimPoint = ResolveTargetAimPoint();
    if (!HasClearShotToTarget(ShotStart, AimPoint))
    {
        BurstShotsRemaining = 0;
        NextBurstTime = FMath::Max(NextBurstTime, Now + 0.03);
        TickAcquireFiringLane(DeltaTime, Now);
        return;
    }

    if (bDamageThreat)
    {
        Awareness->RefreshDamageThreat(
            Target->GetActorLocation(),
            Config->CombatSettings.DamageThreatMemorySeconds,
            Config->PatrolSettings.SearchDurationSeconds,
            Now);
    }

    if (bFiringLaneMovePrepared)
    {
        Locomotion->Stop();
        ResetFiringLaneMove();
    }

    if (!Locomotion->IsFacing(
        AimPoint, Config->ReactionSettings.FireFacingToleranceDegrees))
    {
        BurstShotsRemaining = 0;
        NextBurstTime = FMath::Max(NextBurstTime, Now + 0.03);
        return;
    }

    const FCQBCombatSettings& Combat = Config->CombatSettings;
    if (BurstShotsRemaining <= 0)
    {
        if (Now < NextBurstTime)
        {
            return;
        }

        const int32 MinShots = FMath::Max(1, FMath::Min(Combat.BurstMinShots, Combat.BurstMaxShots));
        const int32 MaxShots = FMath::Max(MinShots, FMath::Max(Combat.BurstMinShots, Combat.BurstMaxShots));
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

    if (BurstShotsRemaining > 0)
    {
        NextShotTime = Now + FMath::FRandRange(
            FMath::Min(Combat.ShotIntervalMinSeconds, Combat.ShotIntervalMaxSeconds),
            FMath::Max(Combat.ShotIntervalMinSeconds, Combat.ShotIntervalMaxSeconds));
    }
    else
    {
        NextBurstTime = Now + FMath::FRandRange(
            FMath::Min(Combat.BurstCooldownMinSeconds, Combat.BurstCooldownMaxSeconds),
            FMath::Max(Combat.BurstCooldownMinSeconds, Combat.BurstCooldownMaxSeconds));
    }
}

void UCQBTrainingCombatComponent::ExitCombat()
{
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
    const UCQBTrainingAIComponent* Config = GetConfig();
    const UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    const AActor* Target = Awareness ? Awareness->GetTargetActor() : nullptr;
    if (!Config || !Target)
    {
        return FVector::ZeroVector;
    }

    const FVector TargetLocation = Awareness->HasVisibleTarget()
        ? Target->GetActorLocation() : Awareness->GetLastKnownTargetLocation();
    return TargetLocation + FVector(0.0f, 0.0f, Config->AccuracySettings.AimPointHeightOffset);
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
        Config->GetActorLocation(), Target->GetActorLocation()) / 100.0f;
    const float ExtraTens = FMath::Max(0.0f, DistanceMeters - 5.0f) / 10.0f;
    Error += ExtraTens * FMath::Max(0.0f, Accuracy.DistancePenaltyPer10Meters);
    Error += static_cast<float>(BurstShotIndex)
        * FMath::Max(0.0f, Accuracy.BurstRecoilDegreesPerShot);
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

    const bool bReached = bFiringLaneMovePrepared
        && Locomotion->TickPreparedMove(DeltaTime, 0.0f);
    if (bReached && !Awareness->HasVisibleTarget() && Awareness->HasDamageThreat(NowSeconds))
    {
        Awareness->CompleteDamagePursuit(
            Config->PatrolSettings.SearchDurationSeconds, NowSeconds);
        Locomotion->Stop();
        ResetFiringLaneMove();
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
    const double Now = World ? World->GetTimeSeconds() : -1.0;
    if (!Controller || !Config || !Awareness || !World
        || !Controller->IsCombatTargetValid(Target)
        || (!Awareness->HasVisibleTarget() && !Awareness->HasDamageThreat(Now)))
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

    if (Config->CombatSettings.bDrawDebugShots)
    {
        DrawDebugLine(World, Start, End, bHit ? FColor::Green : FColor::Red,
            false, 0.75f, 0, 1.2f);
    }
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
