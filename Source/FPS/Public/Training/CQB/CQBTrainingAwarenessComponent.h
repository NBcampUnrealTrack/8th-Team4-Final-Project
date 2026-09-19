#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CQBTrainingAwarenessComponent.generated.h"

class AActor;
struct FCQBReactionSettings;

UCLASS(ClassGroup = (Training))
class FPS_API UCQBTrainingAwarenessComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCQBTrainingAwarenessComponent();

    void ResetRuntimeState();

    void RecordSightAcquired(AActor& Actor, const FVector& SeenLocation,
        const FCQBReactionSettings& ReactionSettings, double NowSeconds);
    void RecordSightLost(AActor& Actor, const FVector& LostLocation,
        float SearchDurationSeconds, double NowSeconds);
    void RecordNoiseHeard(AActor& Actor, const FVector& NoiseLocation,
        float InvestigateTimeoutSeconds, double NowSeconds);
    void RecordDamageThreat(AActor& Actor, const FVector& KnownLocation,
        float ThreatMemorySeconds, float SearchDurationSeconds, double NowSeconds);

    void RefreshVisibleTargetLocation();
    void RefreshDamageThreat(const FVector& KnownLocation, float ThreatMemorySeconds,
        float SearchDurationSeconds, double NowSeconds);
    void CompleteDamagePursuit(float SearchDurationSeconds, double NowSeconds);
    void CompleteInvestigation(float SearchDurationSeconds, double NowSeconds);
    void ClearNoise();
    void ClearSearch();

    bool HasVisibleTarget() const;
    bool HasDamageThreat(double NowSeconds) const;
    bool IsReactionReady(double NowSeconds) const;
    bool HasPendingNoise() const { return bNoisePending; }
    bool HasSearchLocation() const { return bHasSearchLocation; }

    AActor* GetTargetActor() const { return TargetActor.Get(); }
    const FVector& GetLastSeenLocation() const { return LastSeenLocation; }
    const FVector& GetLastKnownTargetLocation() const { return LastKnownTargetLocation; }
    const FVector& GetLastHeardLocation() const { return LastHeardLocation; }
    const FVector& GetSearchLocation() const { return SearchLocation; }
    double GetSightAcquiredTime() const { return SightAcquiredTime; }
    double GetDamageThreatExpireTime() const { return DamageThreatExpireTime; }
    double GetNoiseExpireTime() const { return NoiseExpireTime; }
    double GetSearchExpireTime() const { return SearchExpireTime; }

private:
    void BeginSearch(const FVector& Location, float DurationSeconds, double NowSeconds);

    TWeakObjectPtr<AActor> TargetActor;
    bool bTargetVisible = false;
    FVector LastSeenLocation = FVector::ZeroVector;
    FVector LastKnownTargetLocation = FVector::ZeroVector;
    FVector LastSightLostLocation = FVector::ZeroVector;
    double LastSightLostTime = -1.0;
    double SightAcquiredTime = -1.0;
    double ReactionReadyTime = -1.0;
    int32 SameAnglePeekCount = 0;

    bool bDamageThreatActive = false;
    double DamageThreatExpireTime = -1.0;

    bool bNoisePending = false;
    FVector LastHeardLocation = FVector::ZeroVector;
    double NoiseExpireTime = -1.0;

    bool bHasSearchLocation = false;
    FVector SearchLocation = FVector::ZeroVector;
    double SearchExpireTime = -1.0;
};
