#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/CQB/CQBTrainingAwarenessTypes.h"
#include "CQBTrainingAwarenessComponent.generated.h"

class AActor;
class UCQBTrainingAIComponent;
struct FCQBCombatIncident;
struct FCQBReactionSettings;

UCLASS(ClassGroup = (Training))
class FPS_API UCQBTrainingAwarenessComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCQBTrainingAwarenessComponent();

    void Configure(const UCQBTrainingAIComponent& Config);
    void StartListening();
    void StopListening();
    void ResetRuntimeState();

    void RecordCombatIncident(const FCQBCombatIncident& Incident);
    void RecordDirectionalDamage(const FVector& HitOrigin, const FVector& ThreatDirection,
        float ThreatMemorySeconds, float SearchDurationSeconds, double NowSeconds);

    void RecordSightAcquired(AActor& Actor, const FVector& SeenLocation,
        const FCQBReactionSettings& ReactionSettings, double NowSeconds);
    void RecordSightLost(AActor& Actor, const FVector& LostLocation,
        float SearchDurationSeconds, double NowSeconds);
    void RecordNoiseHeard(AActor& Actor, const FVector& NoiseLocation,
        float InvestigateTimeoutSeconds, double NowSeconds);
    void RecordDamageThreat(AActor& Actor, const FVector& KnownLocation,
        float ThreatMemorySeconds, float SearchDurationSeconds, double NowSeconds);

    void RefreshVisibleTargetLocation();
    void CompleteTacticalObservation(float SearchDurationSeconds, double NowSeconds);
    void CompleteInvestigation(float SearchDurationSeconds, double NowSeconds);
    void ClearNoise();
    void ClearSearch();

    bool HasVisibleTarget() const;
    bool HasDamageThreat(double NowSeconds) const;
    bool IsReactionReady(double NowSeconds) const;
    bool HasPendingNoise() const;
    bool HasSearchLocation() const;

    bool HasThreatKnowledge() const;
    ECQBThreatKnowledge GetThreatKnowledge() const;
    const FVector& GetThreatLocation() const { return LastKnownTargetLocation; }
    uint32 GetEvidenceRevision() const { return EvidenceRevision; }
    float GetConfidence() const;
    double GetEvidenceTime() const { return EvidenceTime; }

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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void BeginSearch(const FVector& Location, float DurationSeconds, double NowSeconds);
    void RecordEvidence(ECQBThreatKnowledge Knowledge, const FVector& Location,
        float Confidence, double NowSeconds);
    FVector EstimateThreatLocation(const FVector& Origin, const FVector& Direction) const;
    void ReleaseExhaustedEvidence();

    TWeakObjectPtr<const UCQBTrainingAIComponent> Configuration;
    FCQBAwarenessSettings Settings;
    ECQBThreatKnowledge ThreatKnowledge = ECQBThreatKnowledge::Unknown;
    uint32 EvidenceRevision = 0;
    float InitialConfidence = 0.0f;
    double EvidenceTime = -1.0;
    double EvidenceExpireTime = -1.0;

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
