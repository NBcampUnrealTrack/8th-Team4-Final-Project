#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CQBTrainingCombatComponent.generated.h"

class ACQBTrainingAIController;
class UCQBTrainingAIComponent;
class UCQBTrainingAwarenessComponent;
class UCQBTrainingLocomotionComponent;

UCLASS(ClassGroup = (Training))
class FPS_API UCQBTrainingCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCQBTrainingCombatComponent();

    void Configure(UCQBTrainingAIComponent& InConfig,
        UCQBTrainingAwarenessComponent& InAwareness,
        UCQBTrainingLocomotionComponent& InLocomotion);
    void ResetRuntimeState();
    void EnterReaction();
    void TickReaction(float DeltaTime);
    void EnterEngage();
    void TickEngage(float DeltaTime);
    void ExitCombat();
    void NotifyPawnHit();

private:
    ACQBTrainingAIController* GetController() const;
    UCQBTrainingAIComponent* GetConfig() const;
    UCQBTrainingAwarenessComponent* GetAwareness() const;
    UCQBTrainingLocomotionComponent* GetLocomotion() const;

    FVector ResolveShotStart() const;
    FVector ResolveTargetAimPoint() const;
    float ComputeAimErrorDegrees() const;
    bool HasClearShotToTarget() const;
    bool TickAcquireFiringLane(float DeltaTime, double NowSeconds);
    void ResetFiringLaneMove();
    void UpdateCombatFacing(float DeltaTime);
    void FireOneShot();
    void ApplyShotDamage(const FHitResult& HitResult);

    TWeakObjectPtr<UCQBTrainingAIComponent> ConfigRef;
    TWeakObjectPtr<UCQBTrainingAwarenessComponent> AwarenessRef;
    TWeakObjectPtr<UCQBTrainingLocomotionComponent> LocomotionRef;

    int32 BurstShotsRemaining = 0;
    int32 BurstShotIndex = 0;
    double NextShotTime = 0.0;
    double NextBurstTime = 0.0;
    double HitPenaltyUntil = -1.0;
    bool bFiringLaneMovePrepared = false;
    FVector FiringLaneSourceLocation = FVector::ZeroVector;
    double NextFiringLanePrepareTime = 0.0;
};
