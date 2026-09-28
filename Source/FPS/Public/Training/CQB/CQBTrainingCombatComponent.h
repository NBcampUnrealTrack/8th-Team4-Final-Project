#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/CQB/CQBTrainingTacticalTypes.h"
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

    bool HasTacticalOpportunity() const;
    bool IsTacticalEngagementActive() const { return bEngagementActive; }
    void EvaluateTacticalIntent();
    ECQBTacticalIntent GetTacticalIntent() const { return TacticalIntent; }
    void TickTacticalIntent(float DeltaTime, ECQBTacticalIntent ExpectedIntent);
    bool GetReservedCoverLocation(FVector& OutLocation) const;

private:
    enum class EPeekPhase : uint8 { None, MoveOut, Exposed, Return };

    ACQBTrainingAIController* GetController() const;
    UCQBTrainingAIComponent* GetConfig() const;
    UCQBTrainingAwarenessComponent* GetAwareness() const;
    UCQBTrainingLocomotionComponent* GetLocomotion() const;

    FVector ResolveShotStart() const;
    FVector ResolveTargetAimPoint() const;
    float ComputeAimErrorDegrees() const;
    bool HasClearShotToTarget(const FVector& Start, const FVector& AimPoint) const;
    bool TickAcquireFiringLane(float DeltaTime, double NowSeconds);
    void ResetFiringLaneMove();
    void UpdateCombatFacing(float DeltaTime);
    void TickFire(double NowSeconds, bool bMoving);
    void FireOneShot(const FVector& Start, const FVector& AimPoint);
    void ApplyShotDamage(const FHitResult& HitResult);

    void ResetTactics();
    void FinishDefensiveEpisode();
    void SetTacticalIntent(ECQBTacticalIntent Intent, double NowSeconds);
    bool FindCover(bool bReposition, FCQBCoverPosition& OutCover) const;
    bool EvaluateCoverAt(const FVector& Location, FCQBCoverPosition& OutCover) const;
    bool IsGeometryBlocking(const FVector& Start, const FVector& End) const;
    void CacheNearbyOccupancy();
    bool IsCoverOccupied(const FVector& Location) const;
    bool FindPeek(const FVector& CoverLocation, const FVector& Direction, FVector& OutPeek) const;
    FVector GetFeetLocation() const;
    FVector GetCoverAimPoint(const FVector& FeetLocation) const;
    bool BeginCoverMove(const FCQBCoverPosition& Cover, ECQBTacticalIntent Intent, double NowSeconds);
    bool BeginTacticalMove(const FVector& Goal, double NowSeconds, float TravelDistance);
    bool TickTacticalMove(float DeltaTime, double NowSeconds);
    void BeginPeek(double NowSeconds);
    void BeginPeekReturn(double NowSeconds);
    void FinishPeek(double NowSeconds);
    void AbandonCover(double NowSeconds);
    void DrawTacticalDebug() const;

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

    ECQBTacticalIntent TacticalIntent = ECQBTacticalIntent::DirectFire;
    EPeekPhase PeekPhase = EPeekPhase::None;
    FCQBCoverPosition SelectedCover;
    TArray<FVector> RecentCoverLocations;
    TArray<FVector> NearbyAllyLocations;
    TArray<FVector> NearbyCoverReservations;
    double NextTacticalEvaluationTime = 0.0;
    double NextCoverQueryTime = 0.0;
    double PositionStartTime = 0.0;
    double NextRepositionTime = 0.0;
    double PeekExposeUntil = 0.0;
    double TacticalMoveUntil = 0.0;
    double DefensiveHoldUntil = 0.0;
    double NextPeekTime = 0.0;
    uint32 EvaluatedEvidenceRevision = MAX_uint32;
    uint32 DefensiveEvidenceRevision = MAX_uint32;
    int32 BurstsFromPosition = 0;
    int32 PreviousPeekSide = 0;
    int32 RepeatedPeekCount = 0;
    bool bEngagementActive = false;
    bool bTacticalMovePrepared = false;
    bool bDefensiveEpisodeActive = false;
    bool bCoverChosenWithSight = false;
};
