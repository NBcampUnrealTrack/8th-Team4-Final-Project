#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/CQB/CQBTrainingAIComponent.h"
#include "CQBTrainingPatrolComponent.generated.h"

class APawn;
class UCQBTrainingLocomotionComponent;

UCLASS(ClassGroup = (Training))
class FPS_API UCQBTrainingPatrolComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCQBTrainingPatrolComponent();

    bool ConfigureRoute(const FCQBPatrolRouteDefinition& RouteDefinition,
        const FCQBPatrolSettings& InSettings, UCQBTrainingLocomotionComponent& InLocomotion);
    void ResetRuntimeState();
    void EnterPatrol();
    void TickPatrol(float DeltaTime);
    void ExitPatrol();
    bool HasRoute() const { return RouteLocations.Num() >= 2; }

private:
    enum class EPatrolPhase : uint8
    {
        Inactive,
        TurningToWaypoint,
        Moving,
        Observing,
    };

    UCQBTrainingLocomotionComponent* GetLocomotion() const;
    APawn* GetControlledPawn() const;
    bool PrepareCurrentWaypoint();
    void BeginTurnToCurrentWaypoint();
    void BeginObservation();
    void AdvanceWaypoint();
    void UpdateObservation(float DeltaTime);

    TWeakObjectPtr<UCQBTrainingLocomotionComponent> LocomotionRef;
    FCQBPatrolSettings Settings;
    TArray<FVector> RouteLocations;
    bool bPingPong = true;
    int32 CurrentWaypointIndex = INDEX_NONE;
    bool bForward = true;
    EPatrolPhase Phase = EPatrolPhase::Inactive;

    double ObservationEndTime = 0.0;
    double NextObservationTurnTime = 0.0;
    float ObservationBaseYaw = 0.0f;
    float ObservationTargetYaw = 0.0f;
    int32 ObservationStep = 0;
};
