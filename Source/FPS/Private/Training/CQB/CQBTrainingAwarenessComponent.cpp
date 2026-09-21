#include "Training/CQB/CQBTrainingAwarenessComponent.h"

#include "Training/CQB/CQBTrainingAIComponent.h"
#include "GameFramework/Actor.h"

UCQBTrainingAwarenessComponent::UCQBTrainingAwarenessComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCQBTrainingAwarenessComponent::ResetRuntimeState()
{
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

    if (LastSightLostTime >= 0.0
        && NowSeconds - LastSightLostTime <= FMath::Max(0.0f, ReactionSettings.SameAngleMemorySeconds)
        && FVector::DistSquared(Actor.GetActorLocation(), LastSightLostLocation)
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
    BeginSearch(LostLocation, SearchDurationSeconds, NowSeconds);
}

void UCQBTrainingAwarenessComponent::RecordNoiseHeard(
    AActor& Actor,
    const FVector& NoiseLocation,
    const float InvestigateTimeoutSeconds,
    const double NowSeconds)
{
    if (HasVisibleTarget() || HasDamageThreat(NowSeconds))
    {
        return;
    }

    TargetActor = &Actor;
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

void UCQBTrainingAwarenessComponent::RefreshDamageThreat(
    const FVector& KnownLocation,
    const float ThreatMemorySeconds,
    const float SearchDurationSeconds,
    const double NowSeconds)
{
    if (!TargetActor.IsValid())
    {
        return;
    }

    LastKnownTargetLocation = KnownLocation;
    bDamageThreatActive = true;
    DamageThreatExpireTime = NowSeconds + FMath::Max(0.5f, ThreatMemorySeconds);
    BeginSearch(
        KnownLocation,
        FMath::Max(0.5f, ThreatMemorySeconds) + FMath::Max(0.5f, SearchDurationSeconds),
        NowSeconds);
}

void UCQBTrainingAwarenessComponent::CompleteDamagePursuit(
    const float SearchDurationSeconds,
    const double NowSeconds)
{
    if (!bDamageThreatActive)
    {
        return;
    }

    bDamageThreatActive = false;
    DamageThreatExpireTime = -1.0;
    BeginSearch(LastKnownTargetLocation, SearchDurationSeconds, NowSeconds);
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
}

bool UCQBTrainingAwarenessComponent::HasVisibleTarget() const
{
    return bTargetVisible && TargetActor.IsValid();
}

bool UCQBTrainingAwarenessComponent::HasDamageThreat(const double NowSeconds) const
{
    return bDamageThreatActive
        && TargetActor.IsValid()
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
    SearchExpireTime = NowSeconds + FMath::Max(0.5f, DurationSeconds);
}
