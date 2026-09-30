#include "Training/CQB/CQBTrainingPatrolComponent.h"

#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Training/CQB/CQBTrainingLocomotionComponent.h"

UCQBTrainingPatrolComponent::UCQBTrainingPatrolComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UCQBTrainingPatrolComponent::ConfigureRoute(
    const FCQBPatrolRouteDefinition& RouteDefinition,
    const FCQBPatrolSettings& InSettings,
    UCQBTrainingLocomotionComponent& InLocomotion)
{
    LocomotionRef = &InLocomotion;
    Settings = InSettings;
    bPingPong = RouteDefinition.bPingPong;
    RouteLocations.Reset();

    if (RouteDefinition.bHasSpawnOrigin)
    {
        RouteLocations.Add(RouteDefinition.SpawnOrigin);
    }
    RouteLocations.Append(RouteDefinition.PatrolLocations);

    ResetRuntimeState();
    return HasRoute();
}

void UCQBTrainingPatrolComponent::ResetRuntimeState()
{
    CurrentWaypointIndex = RouteLocations.Num() >= 2 ? 1 : INDEX_NONE;
    bForward = true;
    Phase = EPatrolPhase::Inactive;
    ObservationEndTime = 0.0;
    NextObservationTurnTime = 0.0;
    ObservationBaseYaw = 0.0f;
    ObservationTargetYaw = 0.0f;
    ObservationStep = 0;
}

void UCQBTrainingPatrolComponent::EnterPatrol()
{
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    if (!Locomotion || !HasRoute() || !RouteLocations.IsValidIndex(CurrentWaypointIndex))
    {
        return;
    }

    Locomotion->Stop();
    Locomotion->SetMoveSpeed(Settings.PatrolSpeed);

    if (!PrepareCurrentWaypoint())
    {
        Phase = EPatrolPhase::TurningToWaypoint;
        return;
    }

    if (Locomotion->HasReachedPreparedGoal())
    {
        BeginObservation();
        return;
    }

    Phase = EPatrolPhase::TurningToWaypoint;
}

void UCQBTrainingPatrolComponent::TickPatrol(const float DeltaTime)
{
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    if (!Locomotion || !HasRoute() || !RouteLocations.IsValidIndex(CurrentWaypointIndex))
    {
        return;
    }

    switch (Phase)
    {
    case EPatrolPhase::TurningToWaypoint:
        if (!Locomotion->HasPreparedGoal() && !PrepareCurrentWaypoint())
        {
            return;
        }
        if (Locomotion->HasReachedPreparedGoal())
        {
            BeginObservation();
            return;
        }

        Locomotion->TurnToward(
            Locomotion->GetPreparedGoal(), DeltaTime, Settings.PatrolTurnSpeedDegreesPerSecond);
        if (Locomotion->IsFacing(
            Locomotion->GetPreparedGoal(), Settings.DepartureFacingToleranceDegrees)
            && Locomotion->RequestPreparedMove())
        {
            Phase = EPatrolPhase::Moving;
        }
        break;

    case EPatrolPhase::Moving:
        if (Locomotion->TickPreparedMove(DeltaTime, Settings.PatrolTurnSpeedDegreesPerSecond))
        {
            BeginObservation();
        }
        break;

    case EPatrolPhase::Observing:
        UpdateObservation(DeltaTime);
        break;

    case EPatrolPhase::Inactive:
    default:
        EnterPatrol();
        break;
    }
}

void UCQBTrainingPatrolComponent::ExitPatrol()
{
    if (UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion())
    {
        Locomotion->Stop();
    }
    Phase = EPatrolPhase::Inactive;
}

UCQBTrainingLocomotionComponent* UCQBTrainingPatrolComponent::GetLocomotion() const
{
    return LocomotionRef.Get();
}

APawn* UCQBTrainingPatrolComponent::GetControlledPawn() const
{
    const AAIController* Controller = Cast<AAIController>(GetOwner());
    return Controller ? Controller->GetPawn() : nullptr;
}

bool UCQBTrainingPatrolComponent::PrepareCurrentWaypoint()
{
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    return Locomotion
        && RouteLocations.IsValidIndex(CurrentWaypointIndex)
        && Locomotion->PrepareMoveGoal(
            RouteLocations[CurrentWaypointIndex], Settings.AcceptanceRadius);
}

void UCQBTrainingPatrolComponent::BeginTurnToCurrentWaypoint()
{
    if (UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion())
    {
        Locomotion->Stop();
        PrepareCurrentWaypoint();
    }
    Phase = EPatrolPhase::TurningToWaypoint;
}

void UCQBTrainingPatrolComponent::BeginObservation()
{
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    APawn* Pawn = GetControlledPawn();
    UWorld* World = GetWorld();
    if (!Locomotion || !Pawn || !World)
    {
        return;
    }

    Locomotion->Stop();

    const float MinPause = FMath::Max(
        0.0f, FMath::Min(Settings.WaypointPauseMinSeconds, Settings.WaypointPauseMaxSeconds));
    const float MaxPause = FMath::Max(
        MinPause, FMath::Max(Settings.WaypointPauseMinSeconds, Settings.WaypointPauseMaxSeconds));
    const float PauseSeconds = FMath::FRandRange(MinPause, MaxPause);

    ObservationBaseYaw = Pawn->GetActorRotation().Yaw;
    ObservationTargetYaw = ObservationBaseYaw - FMath::Max(0.0f, Settings.LookAroundYawDegrees);
    ObservationStep = 0;

    const double Now = World->GetTimeSeconds();
    ObservationEndTime = Now + PauseSeconds;
    NextObservationTurnTime = Now + PauseSeconds / 3.0;
    Phase = EPatrolPhase::Observing;

    if (PauseSeconds <= KINDA_SMALL_NUMBER)
    {
        AdvanceWaypoint();
        BeginTurnToCurrentWaypoint();
    }
}

void UCQBTrainingPatrolComponent::AdvanceWaypoint()
{
    const int32 Count = RouteLocations.Num();
    if (Count <= 1 || !RouteLocations.IsValidIndex(CurrentWaypointIndex))
    {
        return;
    }

    if (!bPingPong)
    {
        CurrentWaypointIndex = (CurrentWaypointIndex + 1) % Count;
        return;
    }

    if (bForward)
    {
        if (CurrentWaypointIndex >= Count - 1)
        {
            bForward = false;
            CurrentWaypointIndex = FMath::Max(0, Count - 2);
        }
        else
        {
            ++CurrentWaypointIndex;
        }
    }
    else if (CurrentWaypointIndex <= 0)
    {
        bForward = true;
        CurrentWaypointIndex = FMath::Min(1, Count - 1);
    }
    else
    {
        --CurrentWaypointIndex;
    }
}

void UCQBTrainingPatrolComponent::UpdateObservation(const float DeltaTime)
{
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    UWorld* World = GetWorld();
    if (!Locomotion || !World)
    {
        return;
    }

    const double Now = World->GetTimeSeconds();
    if (Now >= ObservationEndTime)
    {
        AdvanceWaypoint();
        BeginTurnToCurrentWaypoint();
        return;
    }

    if (ObservationStep < 2 && Now >= NextObservationTurnTime)
    {
        ++ObservationStep;
        const float LookYaw = FMath::Max(0.0f, Settings.LookAroundYawDegrees);
        ObservationTargetYaw = ObservationStep == 1
            ? ObservationBaseYaw + LookYaw
            : ObservationBaseYaw;

        NextObservationTurnTime = ObservationStep == 1
            ? Now + FMath::Max(0.0, ObservationEndTime - Now) * 0.5
            : ObservationEndTime;
    }

    Locomotion->TurnTowardYaw(
        ObservationTargetYaw, DeltaTime, Settings.LookAroundTurnSpeedDegreesPerSecond);
}
