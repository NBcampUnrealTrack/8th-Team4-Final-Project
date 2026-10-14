#include "Training/CQB/CQBTrainingAwarenessComponent.h"

#include "Training/CQB/CQBTrainingAIComponent.h"
#include "Training/CQB/CQBCombatIncidentSubsystem.h"
#include "Training/CQB/CQBTrainingSpawner.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GAS/RogueTargetFilter.h"
#include "NavigationSystem.h"

UCQBTrainingAwarenessComponent::UCQBTrainingAwarenessComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCQBTrainingAwarenessComponent::Configure(const UCQBTrainingAIComponent& Config)
{
    StopListening();
    Configuration = &Config;
    Settings = Config.AwarenessSettings;
    ResetRuntimeState();
}

void UCQBTrainingAwarenessComponent::StartListening()
{
    const UCQBTrainingAIComponent* Config = Configuration.Get();
    APawn* Pawn = Config ? Cast<APawn>(Config->GetOwner()) : nullptr;
    AActor* Encounter = Config ? Config->GetTrainingSpawner() : nullptr;
    if (Pawn && Encounter)
    {
        GetWorld()->GetSubsystem<UCQBCombatIncidentSubsystem>()->RegisterListener(
            *this, *Pawn, *Encounter, Settings.AllyDeathAwarenessRadius);
    }
}

void UCQBTrainingAwarenessComponent::StopListening()
{
    if (UWorld* World = GetWorld())
    {
        if (UCQBCombatIncidentSubsystem* Incidents = World->GetSubsystem<UCQBCombatIncidentSubsystem>())
        {
            Incidents->UnregisterListener(*this);
        }
    }
}

void UCQBTrainingAwarenessComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopListening();
    Super::EndPlay(EndPlayReason);
}

void UCQBTrainingAwarenessComponent::ResetRuntimeState()
{
    ThreatKnowledge = ECQBThreatKnowledge::Unknown;
    EvidenceRevision = 0;
    InitialConfidence = 0.0f;
    EvidenceTime = -1.0;
    EvidenceExpireTime = -1.0;
    TargetActor.Reset();
    bTargetVisible = false;
    LastSeenLocation = FVector::ZeroVector;
    LastKnownTargetLocation = FVector::ZeroVector;
    LastSightLostLocation = FVector::ZeroVector;
    LastSightLostTime = -1.0;
    SightAcquiredTime = -1.0;
    ReactionReadyTime = -1.0;
    SameAnglePeekCount = 0;

    bDamageThreatActive = false;
    DamageThreatExpireTime = -1.0;

    bNoisePending = false;
    LastHeardLocation = FVector::ZeroVector;
    NoiseExpireTime = -1.0;

    bHasSearchLocation = false;
    SearchLocation = FVector::ZeroVector;
    SearchExpireTime = -1.0;
}

void UCQBTrainingAwarenessComponent::RecordSightAcquired(
    AActor& Actor,
    const FVector& SeenLocation,
    const FCQBReactionSettings& ReactionSettings,
    const double NowSeconds)
{
    const bool bSameTarget = TargetActor.Get() == &Actor;
    const bool bWasVisible = bTargetVisible && bSameTarget;
    if (!bSameTarget)
    {
        bDamageThreatActive = false;
        DamageThreatExpireTime = -1.0;
    }

    TargetActor = &Actor;
    bTargetVisible = true;
    LastSeenLocation = SeenLocation;
    LastKnownTargetLocation = SeenLocation;

    if (bWasVisible)
    {
        return;
    }

    RecordEvidence(ECQBThreatKnowledge::Confirmed, SeenLocation, 1.0f, NowSeconds);

    if (LastSightLostTime >= 0.0
        && NowSeconds - LastSightLostTime <= FMath::Max(0.0f, ReactionSettings.SameAngleMemorySeconds)
        && FVector::DistSquared(SeenLocation, LastSightLostLocation)
            <= FMath::Square(FMath::Max(0.0f, ReactionSettings.SameAngleDistance)))
    {
        ++SameAnglePeekCount;
    }
    else
    {
        SameAnglePeekCount = 0;
    }

    const float BaseReaction = FMath::FRandRange(
        FMath::Min(ReactionSettings.ReactionMinSeconds, ReactionSettings.ReactionMaxSeconds),
        FMath::Max(ReactionSettings.ReactionMinSeconds, ReactionSettings.ReactionMaxSeconds));
    const float RepeekMultiplier = FMath::Pow(
        FMath::Clamp(ReactionSettings.RepeekReactionMultiplier, 0.1f, 1.0f),
        static_cast<float>(SameAnglePeekCount));
    const float FinalReaction = FMath::Max(
        FMath::Max(0.0f, ReactionSettings.MinimumReactionSeconds),
        BaseReaction * RepeekMultiplier);

    SightAcquiredTime = NowSeconds;
    ReactionReadyTime = NowSeconds + FinalReaction;
    ClearNoise();
    ClearSearch();
}

void UCQBTrainingAwarenessComponent::RecordSightLost(
    AActor& Actor,
    const FVector& LostLocation,
    const float SearchDurationSeconds,
    const double NowSeconds)
{
    if (TargetActor.Get() != &Actor || !bTargetVisible)
    {
        return;
    }

    bTargetVisible = false;
    LastSeenLocation = LostLocation;
    LastKnownTargetLocation = LostLocation;
    LastSightLostLocation = LostLocation;
    LastSightLostTime = NowSeconds;
    ReactionReadyTime = -1.0;
    RecordEvidence(ECQBThreatKnowledge::LastKnown, LostLocation, 0.85f, NowSeconds);
    BeginSearch(LostLocation, SearchDurationSeconds, NowSeconds);
}

void UCQBTrainingAwarenessComponent::RecordNoiseHeard(
    AActor&,
    const FVector& NoiseLocation,
    const float InvestigateTimeoutSeconds,
    const double NowSeconds)
{
    if (HasVisibleTarget() || HasDamageThreat(NowSeconds) || GetConfidence() > 0.35f)
    {
        return;
    }

    // A sound identifies a location, not a continuously trackable actor.
    TargetActor.Reset();
    bTargetVisible = false;
    bDamageThreatActive = false;
    DamageThreatExpireTime = -1.0;
    RecordEvidence(ECQBThreatKnowledge::Suspected, NoiseLocation, 0.35f, NowSeconds);
    LastHeardLocation = NoiseLocation;
    bNoisePending = true;
    NoiseExpireTime = NowSeconds + FMath::Max(0.5f, InvestigateTimeoutSeconds);
    ClearSearch();
}

void UCQBTrainingAwarenessComponent::RecordDamageThreat(
    AActor& Actor,
    const FVector& KnownLocation,
    const float ThreatMemorySeconds,
    const float SearchDurationSeconds,
    const double NowSeconds)
{
    if (TargetActor.Get() != &Actor)
    {
        bTargetVisible = false;
    }

    TargetActor = &Actor;
    LastKnownTargetLocation = KnownLocation;
    RecordEvidence(HasVisibleTarget() ? ECQBThreatKnowledge::Confirmed : ECQBThreatKnowledge::Suspected,
        KnownLocation, 0.9f, NowSeconds);
    SightAcquiredTime = NowSeconds;
    ReactionReadyTime = NowSeconds;
    bDamageThreatActive = true;
    DamageThreatExpireTime = NowSeconds + FMath::Max(0.5f, ThreatMemorySeconds);

    ClearNoise();
    BeginSearch(
        KnownLocation,
        FMath::Max(0.5f, ThreatMemorySeconds) + FMath::Max(0.5f, SearchDurationSeconds),
        NowSeconds);
}

void UCQBTrainingAwarenessComponent::RefreshVisibleTargetLocation()
{
    if (HasVisibleTarget())
    {
        LastSeenLocation = TargetActor->GetActorLocation();
        LastKnownTargetLocation = LastSeenLocation;
    }
}

void UCQBTrainingAwarenessComponent::CompleteInvestigation(
    const float SearchDurationSeconds,
    const double NowSeconds)
{
    if (!bNoisePending)
    {
        return;
    }

    const FVector InvestigatedLocation = LastHeardLocation;
    ClearNoise();
    BeginSearch(InvestigatedLocation, SearchDurationSeconds, NowSeconds);
}

void UCQBTrainingAwarenessComponent::CompleteTacticalObservation(
    const float SearchDurationSeconds, const double NowSeconds)
{
    if (HasVisibleTarget() || !HasThreatKnowledge() || bNoisePending)
    {
        return;
    }

    bDamageThreatActive = false;
    DamageThreatExpireTime = -1.0;
    BeginSearch(LastKnownTargetLocation, SearchDurationSeconds, NowSeconds);
}

void UCQBTrainingAwarenessComponent::ClearNoise()
{
    bNoisePending = false;
    NoiseExpireTime = -1.0;
}

void UCQBTrainingAwarenessComponent::ClearSearch()
{
    bHasSearchLocation = false;
    SearchLocation = FVector::ZeroVector;
    SearchExpireTime = -1.0;
    ReleaseExhaustedEvidence();
}

bool UCQBTrainingAwarenessComponent::HasVisibleTarget() const
{
    return bTargetVisible && TargetActor.IsValid() && URogueTargetFilter::IsAlive(TargetActor.Get());
}

bool UCQBTrainingAwarenessComponent::HasDamageThreat(const double NowSeconds) const
{
    return bDamageThreatActive
        && HasThreatKnowledge()
        && DamageThreatExpireTime >= 0.0
        && NowSeconds <= DamageThreatExpireTime;
}

bool UCQBTrainingAwarenessComponent::IsReactionReady(const double NowSeconds) const
{
    return HasVisibleTarget() && ReactionReadyTime >= 0.0 && NowSeconds >= ReactionReadyTime;
}

void UCQBTrainingAwarenessComponent::BeginSearch(
    const FVector& Location,
    const float DurationSeconds,
    const double NowSeconds)
{
    SearchLocation = Location;
    bHasSearchLocation = true;
    SearchExpireTime = FMath::Min(EvidenceExpireTime,
        NowSeconds + FMath::Max(0.5f, DurationSeconds));
}

void UCQBTrainingAwarenessComponent::RecordCombatIncident(const FCQBCombatIncident& Incident)
{
    const UCQBTrainingAIComponent* Config = Configuration.Get();
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Config || !Config->IsAlive() || HasVisibleTarget() || HasDamageThreat(Now)
        || GetConfidence() > 0.55f)
    {
        return;
    }

    TargetActor.Reset();
    bTargetVisible = false;
    bDamageThreatActive = false;
    DamageThreatExpireTime = -1.0;
    const FVector Estimate = EstimateThreatLocation(Incident.Origin, Incident.ThreatDirection);
    RecordEvidence(ECQBThreatKnowledge::Suspected, Estimate, 0.55f, Incident.Timestamp);
    LastHeardLocation = Estimate;
    bNoisePending = true;
    NoiseExpireTime = FMath::Min(EvidenceExpireTime,
        Now + FMath::Max(0.5f, Config->PatrolSettings.InvestigateTimeoutSeconds));
    ClearSearch();
}

void UCQBTrainingAwarenessComponent::RecordDirectionalDamage(
    const FVector& HitOrigin, const FVector& ThreatDirection,
    const float ThreatMemorySeconds, const float SearchDurationSeconds,
    const double NowSeconds)
{
    if (HasVisibleTarget())
    {
        RecordDamageThreat(*TargetActor.Get(), LastSeenLocation, ThreatMemorySeconds,
            SearchDurationSeconds, NowSeconds);
        return;
    }

    TargetActor.Reset();
    bTargetVisible = false;
    const FVector Estimate = EstimateThreatLocation(HitOrigin, ThreatDirection);
    RecordEvidence(ECQBThreatKnowledge::Suspected, Estimate, 0.9f, NowSeconds);
    bDamageThreatActive = true;
    DamageThreatExpireTime = NowSeconds + FMath::Max(0.5f, ThreatMemorySeconds);
    ReactionReadyTime = NowSeconds;
    SightAcquiredTime = NowSeconds;
    ClearNoise();
    BeginSearch(Estimate, ThreatMemorySeconds + SearchDurationSeconds, NowSeconds);
}

void UCQBTrainingAwarenessComponent::RecordEvidence(
    const ECQBThreatKnowledge Knowledge, const FVector& Location,
    const float Confidence, const double NowSeconds)
{
    ThreatKnowledge = Knowledge;
    LastKnownTargetLocation = Location;
    InitialConfidence = Confidence;
    EvidenceTime = NowSeconds;
    EvidenceExpireTime = NowSeconds + FMath::Max(1.0f, Settings.EvidenceMemorySeconds);
    ++EvidenceRevision;
}

bool UCQBTrainingAwarenessComponent::HasThreatKnowledge() const
{
    return GetConfidence() > 0.0f;
}

ECQBThreatKnowledge UCQBTrainingAwarenessComponent::GetThreatKnowledge() const
{
    if (HasVisibleTarget())
    {
        return ECQBThreatKnowledge::Confirmed;
    }
    if (!HasThreatKnowledge())
    {
        return ECQBThreatKnowledge::Unknown;
    }
    const ECQBThreatKnowledge RememberedKnowledge = ThreatKnowledge == ECQBThreatKnowledge::Confirmed
        ? ECQBThreatKnowledge::LastKnown : ThreatKnowledge;
    return RememberedKnowledge == ECQBThreatKnowledge::LastKnown && GetConfidence() < 0.4f
        ? ECQBThreatKnowledge::Suspected : RememberedKnowledge;
}

float UCQBTrainingAwarenessComponent::GetConfidence() const
{
    if (HasVisibleTarget())
    {
        return 1.0f;
    }
    const UWorld* World = GetWorld();
    if (!World || EvidenceExpireTime <= EvidenceTime)
    {
        return 0.0f;
    }
    const double RemainingFraction = FMath::Clamp(
        (EvidenceExpireTime - World->GetTimeSeconds()) / (EvidenceExpireTime - EvidenceTime), 0.0, 1.0);
    return InitialConfidence * static_cast<float>(RemainingFraction);
}

bool UCQBTrainingAwarenessComponent::HasPendingNoise() const
{
    return bNoisePending && HasThreatKnowledge();
}

bool UCQBTrainingAwarenessComponent::HasSearchLocation() const
{
    return bHasSearchLocation && HasThreatKnowledge();
}

void UCQBTrainingAwarenessComponent::ReleaseExhaustedEvidence()
{
    const UWorld* World = GetWorld();
    if (!HasVisibleTarget() && !HasPendingNoise()
        && !(bDamageThreatActive && World && World->GetTimeSeconds() <= DamageThreatExpireTime))
    {
        ThreatKnowledge = ECQBThreatKnowledge::Unknown;
        InitialConfidence = 0.0f;
        EvidenceExpireTime = -1.0;
        TargetActor.Reset();
    }
}

FVector UCQBTrainingAwarenessComponent::EstimateThreatLocation(
    const FVector& Origin, const FVector& Direction) const
{
    const FVector FlatDirection = Direction.GetSafeNormal2D();
    // No reliable direction means investigate the casualty, not an invented shooter bearing.
    if (FlatDirection.IsNearlyZero())
    {
        return Origin;
    }

    const float MinimumDistance = FMath::Max(0.0f,
        static_cast<float>(FMath::Min(Settings.ShooterEstimateDistance.X, Settings.ShooterEstimateDistance.Y)));
    const float MaximumDistance = FMath::Max(MinimumDistance,
        static_cast<float>(FMath::Max(Settings.ShooterEstimateDistance.X, Settings.ShooterEstimateDistance.Y)));
    const float Uncertainty = FMath::Clamp(Settings.DirectionalUncertaintyDegrees, 0.0f, 90.0f);
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const UCQBTrainingAIComponent* Config = Configuration.Get();
    const FVector ProjectionExtent = Config
        ? Config->PatrolSettings.NavigationProjectionExtent.GetAbs() : FVector(120.0f, 120.0f, 250.0f);
    const APawn* Pawn = Config ? Cast<APawn>(Config->GetOwner()) : nullptr;
    const double CenterHeight = Pawn
        ? Pawn->GetActorLocation().Z - Pawn->GetNavAgentLocation().Z : 0.0;

    for (int32 Attempt = 0; Attempt < 3; ++Attempt)
    {
        const FVector EstimatedDirection = FlatDirection.RotateAngleAxis(
            FMath::FRandRange(-Uncertainty, Uncertainty), FVector::UpVector);
        const FVector Candidate = Origin + EstimatedDirection * FMath::FRandRange(MinimumDistance, MaximumDistance);
        FNavLocation Projected;
        if (Navigation && Navigation->ProjectPointToNavigation(Candidate, Projected, ProjectionExtent))
        {
            // Aim memory uses actor-center locations; NavMesh supplies ground locations.
            return Projected.Location + FVector(0.0, 0.0, CenterHeight);
        }
    }
    return Origin;
}
