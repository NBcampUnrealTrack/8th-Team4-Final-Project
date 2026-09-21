#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/CQB/CQBTrainingAIComponent.h"
#include "CQBTrainingLocomotionComponent.generated.h"

class AAIController;
class APawn;

UCLASS(ClassGroup = (Training))
class FPS_API UCQBTrainingLocomotionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCQBTrainingLocomotionComponent();

    void Configure(const FCQBPatrolSettings& InSettings);
    void ResetRuntimeState();
    void Stop();
    void SetMoveSpeed(float Speed) const;

    bool PrepareMoveGoal(const FVector& Destination, float AcceptanceRadius);
    bool RequestPreparedMove();
    bool TickPreparedMove(float DeltaTime, float TurnSpeedDegreesPerSecond);
    bool HasReachedPreparedGoal() const;
    bool HasPreparedGoal() const { return bHasMoveGoal; }
    const FVector& GetPreparedGoal() const { return MoveGoal; }

    void UpdateMoveFacing(float DeltaTime, float TurnSpeedDegreesPerSecond);
    bool TurnToward(const FVector& WorldLocation, float DeltaTime,
        float TurnSpeedDegreesPerSecond, bool bAimPitch = false);
    bool TurnTowardYaw(float TargetYaw, float DeltaTime, float TurnSpeedDegreesPerSecond);
    bool IsFacing(const FVector& WorldLocation, float ToleranceDegrees) const;

private:
    AAIController* GetController() const;
    APawn* GetControlledPawn() const;
    bool ResolveNavigationGoal(const FVector& Destination, FVector& OutResolvedGoal) const;
    void ScheduleMoveRetry();
    void LogMoveFailure(const TCHAR* Reason, const FVector& Goal, int32 ResultCode = INDEX_NONE) const;

    FCQBPatrolSettings Settings;
    FVector MoveGoal = FVector::ZeroVector;
    float MoveAcceptanceRadius = 65.0f;
    bool bHasMoveGoal = false;
    bool bMoveRequested = false;
    double NextMoveRetryTime = 0.0;
    mutable double NextMoveFailureLogTime = 0.0;
};
