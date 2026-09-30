#include "Training/CQB/CQBTrainingCombatComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "Training/CQB/CQBTrainingAIComponent.h"
#include "Training/CQB/CQBTrainingAIController.h"
#include "Training/CQB/CQBTrainingAwarenessComponent.h"
#include "Training/CQB/CQBTrainingLocomotionComponent.h"

namespace CQBTactics
{
    constexpr float CoverAcceptanceRadius = 25.0f;
    constexpr float CoverSeparation = 160.0f;
    constexpr int32 MaxPathQueries = 4;
    constexpr int32 MaxRecentCovers = 3;
}

void UCQBTrainingCombatComponent::ResetTactics()
{
    TacticalIntent = ECQBTacticalIntent::DirectFire;
    PeekPhase = EPeekPhase::None;
    SelectedCover = FCQBCoverPosition();
    RecentCoverLocations.Reset();
    RecentCoverWriteIndex = 0;
    NearbyAllyLocations.Reset();
    NearbyCoverReservations.Reset();
    NextTacticalEvaluationTime = 0.0;
    NextCoverQueryTime = 0.0;
    PositionStartTime = 0.0;
    NextRepositionTime = 0.0;
    PeekExposeUntil = 0.0;
    TacticalMoveUntil = 0.0;
    DefensiveHoldUntil = 0.0;
    NextPeekTime = 0.0;
    EvaluatedEvidenceRevision = MAX_uint32;
    DefensiveEvidenceRevision = MAX_uint32;
    BurstsFromPosition = 0;
    PreviousPeekSide = 0;
    RepeatedPeekCount = 0;
    bEngagementActive = false;
    bTacticalMovePrepared = false;
    bDefensiveEpisodeActive = false;
    bCoverChosenWithSight = false;
}

bool UCQBTrainingCombatComponent::HasTacticalOpportunity() const
{
    const UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    if (!Awareness || !Awareness->HasThreatKnowledge())
    {
        return false;
    }
    if (Awareness->HasVisibleTarget())
    {
        return GetController()->IsCombatTargetValid(Awareness->GetTargetActor());
    }
    if (bDefensiveEpisodeActive && Awareness->GetEvidenceRevision() == DefensiveEvidenceRevision)
    {
        return GetWorld()->GetTimeSeconds() < DefensiveHoldUntil;
    }
    if (Awareness->GetConfidence() < 0.4f)
    {
        return false;
    }
    return Awareness->GetEvidenceRevision() != DefensiveEvidenceRevision;
}

void UCQBTrainingCombatComponent::FinishDefensiveEpisode()
{
    if (bDefensiveEpisodeActive)
    {
        bDefensiveEpisodeActive = false;
        const UCQBTrainingAIComponent* Config = GetConfig();
        UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
        const UWorld* World = GetWorld();
        if (Config && Awareness && World)
        {
            Awareness->CompleteTacticalObservation(
                Config->PatrolSettings.SearchDurationSeconds, World->GetTimeSeconds());
        }
    }
}

void UCQBTrainingCombatComponent::SetTacticalIntent(
    const ECQBTacticalIntent Intent, const double NowSeconds)
{
    if (TacticalIntent != Intent)
    {
        BurstShotsRemaining = 0;
        BurstShotIndex = 0;
        NextShotTime = NowSeconds;
    }
    TacticalIntent = Intent;
}

void UCQBTrainingCombatComponent::EvaluateTacticalIntent()
{
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    const UCQBTrainingAIComponent* Config = GetConfig();
    if (!Config || !Awareness || !Awareness->HasThreatKnowledge())
    {
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    const uint32 Revision = Awareness->GetEvidenceRevision();
    const bool bNewEvidence = EvaluatedEvidenceRevision != Revision;
    if (!bNewEvidence && Now < NextTacticalEvaluationTime)
    {
        return;
    }
    EvaluatedEvidenceRevision = Revision;
    NextTacticalEvaluationTime = Now + FMath::Max(0.2f, Config->TacticalSettings.ReevaluationIntervalSeconds);
    const bool bVisible = Awareness->HasVisibleTarget();
    if (bVisible)
    {
        Awareness->RefreshVisibleTargetLocation();
        bDefensiveEpisodeActive = false;
    }
    else if (Revision != DefensiveEvidenceRevision)
    {
        if (!SelectedCover.bValid || PeekPhase == EPeekPhase::Exposed)
        {
            bCoverChosenWithSight = false;
        }
        DefensiveEvidenceRevision = Revision;
        bDefensiveEpisodeActive = true;
        // One finite defensive attempt per piece of evidence, including travel.
        DefensiveHoldUntil = Now + Config->TacticalSettings.HoldAngleSeconds + 2.0f;
    }
    if (!bVisible && (!bDefensiveEpisodeActive || Now >= DefensiveHoldUntil))
    {
        FinishDefensiveEpisode();
        return;
    }
    if (bTacticalMovePrepared || PeekPhase != EPeekPhase::None)
    {
        return;
    }

    CacheNearbyOccupancy();
    const FVector Feet = GetFeetLocation();
    bool bAtCover = SelectedCover.bValid
        && FVector::DistSquared2D(Feet, SelectedCover.Location) <= FMath::Square(45.0f);
    if (bAtCover)
    {
        FCQBCoverPosition Refreshed;
        bAtCover = EvaluateCoverAt(SelectedCover.Location, Refreshed);
        if (bAtCover)
        {
            Refreshed.PathLength = SelectedCover.PathLength;
            SelectedCover = Refreshed;
        }
        else
        {
            SelectedCover.bValid = false;
        }
    }

    const float Age = static_cast<float>(Now - PositionStartTime);
    const float Exposure = IsGeometryBlocking(ResolveTargetAimPoint(), GetCoverAimPoint(Feet)) ? 0.0f : 1.0f;
    const float RecentDamage = Now < HitPenaltyUntil ? 1.0f : 0.0f;
    const float Reuse = static_cast<float>(BurstsFromPosition)
        / FMath::Max(1, Config->TacticalSettings.MaxBurstsPerPosition);
    const float RepositionScore = Exposure + 1.4f * RecentDamage + Reuse
        + FMath::Clamp(Age / FMath::Max(1.0f, Config->TacticalSettings.HoldAngleSeconds * 3.0f), 0.0f, 1.5f)
        + (RepeatedPeekCount >= 2 ? 0.7f : 0.0f);
    const float HoldScore = bAtCover ? 1.2f + SelectedCover.Quality * 0.35f : 0.3f;
    const bool bShouldReposition = bVisible || bCoverChosenWithSight;

    if (bAtCover && bShouldReposition && Now >= NextRepositionTime && RepositionScore > HoldScore
        && Now >= NextCoverQueryTime)
    {
        NextCoverQueryTime = Now + FMath::Max(0.2f, Config->TacticalSettings.ReevaluationIntervalSeconds);
        FCQBCoverPosition Alternative;
        if (FindCover(true, Alternative) && BeginCoverMove(Alternative, ECQBTacticalIntent::Reposition, Now))
        {
            NextRepositionTime = Now + Config->TacticalSettings.RepositionCooldownSeconds;
            return;
        }
        NextRepositionTime = Now + Config->TacticalSettings.RepositionCooldownSeconds;
    }

    if (!bAtCover && Now >= NextCoverQueryTime)
    {
        NextCoverQueryTime = Now + FMath::Max(0.2f, Config->TacticalSettings.ReevaluationIntervalSeconds);
        FCQBCoverPosition Cover;
        if (FindCover(false, Cover) && BeginCoverMove(Cover, ECQBTacticalIntent::TakeCover, Now))
        {
            return;
        }
    }

    if (bAtCover)
    {
        GetLocomotion()->Stop();
        SetTacticalIntent(ECQBTacticalIntent::HoldAngle, Now);
        if (Now >= NextPeekTime && (bVisible || bCoverChosenWithSight)
            && (SelectedCover.bLeftPeek || SelectedCover.bRightPeek))
        {
            BeginPeek(Now);
        }
    }
    else
    {
        SetTacticalIntent(ECQBTacticalIntent::DirectFire, Now);
        if (!bVisible)
        {
            FinishDefensiveEpisode();
        }
    }
}

void UCQBTrainingCombatComponent::TickTacticalIntent(
    const float DeltaTime, const ECQBTacticalIntent ExpectedIntent)
{
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    if (ExpectedIntent != TacticalIntent || !Awareness || !HasTacticalOpportunity())
    {
        return;
    }
    if (Awareness->HasVisibleTarget())
    {
        Awareness->RefreshVisibleTargetLocation();
    }
    UpdateCombatFacing(DeltaTime);
    const double Now = GetWorld()->GetTimeSeconds();
    switch (TacticalIntent)
    {
    case ECQBTacticalIntent::TakeCover:
    case ECQBTacticalIntent::Reposition:
        if (TickTacticalMove(DeltaTime, Now))
        {
            PositionStartTime = Now;
            BurstsFromPosition = 0;
            RepeatedPeekCount = 0;
            NextPeekTime = Now + 0.35;
            SetTacticalIntent(ECQBTacticalIntent::HoldAngle, Now);
            NextTacticalEvaluationTime = 0.0;
        }
        else if (bTacticalMovePrepared)
        {
            TickFire(Now, true);
        }
        break;
    case ECQBTacticalIntent::HoldAngle:
        TickFire(Now, false);
        break;
    case ECQBTacticalIntent::PeekFire:
        if (PeekPhase == EPeekPhase::MoveOut && TickTacticalMove(DeltaTime, Now))
        {
            PeekPhase = EPeekPhase::Exposed;
            PeekExposeUntil = Now + GetConfig()->TacticalSettings.PeekExposureSeconds;
            NextBurstTime = Now;
        }
        if (PeekPhase == EPeekPhase::Exposed)
        {
            if (Now >= PeekExposeUntil || Now < HitPenaltyUntil)
            {
                BeginPeekReturn(Now);
            }
            else
            {
                TickFire(Now, false);
            }
        }
        if (PeekPhase == EPeekPhase::Return && TickTacticalMove(DeltaTime, Now))
        {
            FinishPeek(Now);
        }
        break;
    case ECQBTacticalIntent::DirectFire:
        if (Awareness->HasVisibleTarget())
        {
            if (!HasClearShotToTarget(ResolveShotStart(), ResolveTargetAimPoint()))
            {
                TickAcquireFiringLane(DeltaTime, Now);
            }
            else
            {
                if (bFiringLaneMovePrepared)
                {
                    GetLocomotion()->Stop();
                    ResetFiringLaneMove();
                }
                TickFire(Now, false);
            }
        }
        break;
    }
}

FVector UCQBTrainingCombatComponent::GetFeetLocation() const
{
    return GetController()->GetPawn()->GetNavAgentLocation();
}

FVector UCQBTrainingCombatComponent::GetCoverAimPoint(const FVector& FeetLocation) const
{
    const float Height = FMath::Clamp(static_cast<float>(GetConfig()->GetPawnViewLocation().Z - GetFeetLocation().Z), 100.0f, 200.0f);
    return FeetLocation + FVector(0.0f, 0.0f, Height);
}

bool UCQBTrainingCombatComponent::IsGeometryBlocking(const FVector& Start, const FVector& End) const
{
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBCoverVisibility), false, GetController()->GetPawn());
    Params.AddIgnoredActor(GetController());
    Params.AddIgnoredActor(GetAwareness()->GetTargetActor());
    FHitResult Hit;
    return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)
        && !Cast<APawn>(Hit.GetActor());
}

void UCQBTrainingCombatComponent::CacheNearbyOccupancy()
{
    NearbyAllyLocations.Reset();
    NearbyCoverReservations.Reset();
    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBCoverOccupancy), false, GetController()->GetPawn());
    const float QueryRadius = FMath::Clamp(GetConfig()->TacticalSettings.CoverSearchRadius, 150.0f, 1200.0f)
        + 1200.0f + CQBTactics::CoverSeparation;
    GetWorld()->OverlapMultiByObjectType(Overlaps, GetFeetLocation(),
        FQuat::Identity, Objects, FCollisionShape::MakeSphere(QueryRadius), Params);
    for (const FOverlapResult& Overlap : Overlaps)
    {
        const APawn* Pawn = Cast<APawn>(Overlap.GetActor());
        const ACQBTrainingAIController* Ally = Pawn ? Cast<ACQBTrainingAIController>(Pawn->GetController()) : nullptr;
        if (!Ally)
        {
            continue;
        }
        NearbyAllyLocations.AddUnique(Pawn->GetActorLocation());
        FVector Reserved;
        if (Ally->GetCombatComponent()->GetReservedCoverLocation(Reserved))
        {
            NearbyCoverReservations.AddUnique(Reserved);
        }
    }
}

bool UCQBTrainingCombatComponent::IsCoverOccupied(const FVector& Location) const
{
    for (const FVector& AllyLocation : NearbyAllyLocations)
    {
        if (FVector::DistSquared2D(AllyLocation, Location) < FMath::Square(120.0f))
        {
            return true;
        }
    }
    for (const FVector& Reserved : NearbyCoverReservations)
    {
        if (FVector::DistSquared2D(Reserved, Location) < FMath::Square(CQBTactics::CoverSeparation))
        {
            return true;
        }
    }
    return false;
}

bool UCQBTrainingCombatComponent::GetReservedCoverLocation(FVector& OutLocation) const
{
    if (!bEngagementActive || !SelectedCover.bValid || TacticalIntent == ECQBTacticalIntent::DirectFire)
    {
        return false;
    }
    OutLocation = SelectedCover.Location;
    return true;
}

bool UCQBTrainingCombatComponent::FindPeek(
    const FVector& CoverLocation, const FVector& Direction, FVector& OutPeek) const
{
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const ACharacter* Character = Cast<ACharacter>(GetController()->GetPawn());
    if (!Navigation || !Character)
    {
        return false;
    }
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CQBPeekClearance), false, Character);
    const FVector CenterOffset(0.0f, 0.0f, HalfHeight + 3.0f);
    for (const float Offset : { 110.0f, 200.0f })
    {
        const FVector Desired = CoverLocation + Direction * Offset;
        FNavLocation Projected;
        if (!Navigation->ProjectPointToNavigation(Desired, Projected, FVector(40.0f, 40.0f, 80.0f),
                &Character->GetNavAgentPropertiesRef())
            || FVector::DistSquared2D(Desired, Projected.Location) > FMath::Square(45.0f)
            || FMath::Abs(Projected.Location.Z - CoverLocation.Z) > 50.0f)
        {
            continue;
        }
        FVector NavHit;
        if (UNavigationSystemV1::NavigationRaycast(GetController(), CoverLocation, Projected.Location,
            NavHit, nullptr, GetController()))
        {
            continue;
        }
        FHitResult Hit;
        if (GetWorld()->SweepSingleByChannel(Hit, CoverLocation + CenterOffset, Projected.Location + CenterOffset,
            FQuat::Identity, Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Radius, HalfHeight), Params))
        {
            continue;
        }
        const float ShotHeight = FMath::Clamp(static_cast<float>(ResolveShotStart().Z - GetFeetLocation().Z), 60.0f, 180.0f);
        const FVector FiringPoint = Projected.Location + FVector(0.0f, 0.0f, ShotHeight);
        if (!IsGeometryBlocking(GetCoverAimPoint(Projected.Location), ResolveTargetAimPoint())
            && !IsGeometryBlocking(FiringPoint, ResolveTargetAimPoint())
            && !IsCoverOccupied(Projected.Location))
        {
            OutPeek = Projected.Location;
            return true;
        }
    }
    return false;
}

bool UCQBTrainingCombatComponent::EvaluateCoverAt(const FVector& Location, FCQBCoverPosition& OutCover) const
{
    const FVector AimPoint = ResolveTargetAimPoint();
    const FVector Head = GetCoverAimPoint(Location);
    const FVector Chest = Location + (Head - Location) * 0.65f;
    if (!IsGeometryBlocking(AimPoint, Head) || !IsGeometryBlocking(AimPoint, Chest)
        || IsCoverOccupied(Location))
    {
        return false;
    }
    const FVector TowardThreat = (AimPoint - Location).GetSafeNormal2D();
    const FVector Right(-TowardThreat.Y, TowardThreat.X, 0.0f);
    OutCover.Location = Location;
    OutCover.bLeftPeek = FindPeek(Location, -Right, OutCover.LeftPeek);
    OutCover.bRightPeek = FindPeek(Location, Right, OutCover.RightPeek);
    if (!OutCover.bLeftPeek && !OutCover.bRightPeek)
    {
        return false;
    }
    OutCover.Quality = 1.0f + (OutCover.bLeftPeek && OutCover.bRightPeek ? 0.75f : 0.0f);
    OutCover.bValid = true;
    return true;
}

bool UCQBTrainingCombatComponent::FindCover(const bool bReposition, FCQBCoverPosition& OutCover) const
{
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Navigation)
    {
        return false;
    }
    struct FScoredCover { FCQBCoverPosition Cover; float Score; };
    TArray<FScoredCover, TInlineAllocator<17>> Candidates;
    const FVector Origin = GetFeetLocation();
    const float Radius = FMath::Clamp(GetConfig()->TacticalSettings.CoverSearchRadius, 150.0f, 1200.0f);
    const FVector ThreatDirection = (ResolveTargetAimPoint() - Origin).GetSafeNormal2D();
    for (int32 Index = 0; Index < 17; ++Index)
    {
        const int32 Ring = (Index - 1) / 8;
        const float Angle = Index == 0 ? 0.0f : static_cast<float>((Index - 1) % 8) * 45.0f;
        const FVector Desired = Index == 0 ? Origin
            : Origin + ThreatDirection.RotateAngleAxis(Angle, FVector::UpVector) * Radius * (Ring == 0 ? 0.45f : 0.9f);
        FNavLocation Projected;
        if (!Navigation->ProjectPointToNavigation(Desired, Projected, FVector(65.0f, 65.0f, 100.0f),
                &GetController()->GetPawn()->GetNavAgentPropertiesRef())
            || FVector::DistSquared2D(Desired, Projected.Location) > FMath::Square(70.0f)
            || FMath::Abs(Origin.Z - Projected.Location.Z) > 100.0f
            || (bReposition && FVector::DistSquared2D(Projected.Location, SelectedCover.Location)
                < FMath::Square(CQBTactics::CoverSeparation)))
        {
            continue;
        }
        FCQBCoverPosition Cover;
        if (!EvaluateCoverAt(Projected.Location, Cover))
        {
            continue;
        }
        float Score = Cover.Quality - FVector::Dist2D(Origin, Cover.Location) / Radius * 0.7f;
        bool bRecentlyUsed = false;
        for (const FVector& Recent : RecentCoverLocations)
        {
            if (FVector::DistSquared2D(Recent, Cover.Location) < FMath::Square(CQBTactics::CoverSeparation))
            {
                Score -= 1.5f;
                bRecentlyUsed = true;
            }
        }
        if (!bReposition || !bRecentlyUsed)
        {
            Candidates.Add({ Cover, Score });
        }
    }
    Candidates.Sort([](const FScoredCover& A, const FScoredCover& B) { return A.Score > B.Score; });
    for (int32 Index = 0; Index < FMath::Min(CQBTactics::MaxPathQueries, Candidates.Num()); ++Index)
    {
        FCQBCoverPosition Cover = Candidates[Index].Cover;
        FVector Reachable;
        if (GetLocomotion()->FindReachableLocation(Cover.Location, Reachable, Cover.PathLength, 30.0f, Radius * 2.2f)
            && FVector::DistSquared(Reachable, Cover.Location) <= FMath::Square(35.0f))
        {
            OutCover = Cover;
            return true;
        }
    }
    return false;
}

bool UCQBTrainingCombatComponent::BeginTacticalMove(
    const FVector& Goal, const double NowSeconds, const float TravelDistance)
{
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    Locomotion->Stop();
    ResetFiringLaneMove();
    Locomotion->SetMoveSpeed(GetConfig()->CombatSettings.RepositionSpeed);
    bTacticalMovePrepared = Locomotion->PrepareMoveGoal(Goal, CQBTactics::CoverAcceptanceRadius);
    if (bTacticalMovePrepared)
    {
        Locomotion->RequestPreparedMove();
        TacticalMoveUntil = NowSeconds + FMath::Clamp(1.0f + TravelDistance
            / FMath::Max(50.0f, GetConfig()->CombatSettings.RepositionSpeed) * 2.0f, 1.5f, 6.0f);
    }
    return bTacticalMovePrepared;
}

bool UCQBTrainingCombatComponent::BeginCoverMove(
    const FCQBCoverPosition& Cover, const ECQBTacticalIntent Intent, const double NowSeconds)
{
    if (!BeginTacticalMove(Cover.Location, NowSeconds, Cover.PathLength))
    {
        return false;
    }
    if (SelectedCover.bValid
        && FVector::DistSquared2D(SelectedCover.Location, Cover.Location) > FMath::Square(CQBTactics::CoverSeparation))
    {
        const int32 MaxRecent = FMath::Max(1, CQBTactics::MaxRecentCovers);
        if (RecentCoverLocations.Num() < MaxRecent)
        {
            RecentCoverLocations.Add(SelectedCover.Location);
        }
        else
        {
            RecentCoverLocations[RecentCoverWriteIndex] = SelectedCover.Location;
            RecentCoverWriteIndex = (RecentCoverWriteIndex + 1) % MaxRecent;
        }
    }
    SelectedCover = Cover;
    bCoverChosenWithSight = bCoverChosenWithSight || GetAwareness()->HasVisibleTarget();
    PeekPhase = EPeekPhase::None;
    SetTacticalIntent(Intent, NowSeconds);
    return true;
}

bool UCQBTrainingCombatComponent::TickTacticalMove(const float DeltaTime, const double NowSeconds)
{
    if (!bTacticalMovePrepared)
    {
        return false;
    }
    if (GetLocomotion()->TickPreparedMove(DeltaTime, 0.0f))
    {
        GetLocomotion()->Stop();
        bTacticalMovePrepared = false;
        return true;
    }
    if (NowSeconds >= TacticalMoveUntil)
    {
        AbandonCover(NowSeconds);
    }
    return false;
}

void UCQBTrainingCombatComponent::BeginPeek(const double NowSeconds)
{
    const int32 Side = SelectedCover.bLeftPeek && SelectedCover.bRightPeek
        ? (PreviousPeekSide == 1 ? -1 : 1) : (SelectedCover.bRightPeek ? 1 : -1);
    const FVector Goal = Side == 1 ? SelectedCover.RightPeek : SelectedCover.LeftPeek;
    if (!BeginTacticalMove(Goal, NowSeconds, FVector::Dist2D(GetFeetLocation(), Goal)))
    {
        AbandonCover(NowSeconds);
        return;
    }
    RepeatedPeekCount = Side == PreviousPeekSide ? RepeatedPeekCount + 1 : 1;
    PreviousPeekSide = Side;
    PeekPhase = EPeekPhase::MoveOut;
    SetTacticalIntent(ECQBTacticalIntent::PeekFire, NowSeconds);
}

void UCQBTrainingCombatComponent::BeginPeekReturn(const double NowSeconds)
{
    BurstShotsRemaining = 0;
    if (!BeginTacticalMove(SelectedCover.Location, NowSeconds,
        FVector::Dist2D(GetFeetLocation(), SelectedCover.Location)))
    {
        AbandonCover(NowSeconds);
        return;
    }
    PeekPhase = EPeekPhase::Return;
}

void UCQBTrainingCombatComponent::FinishPeek(const double NowSeconds)
{
    PeekPhase = EPeekPhase::None;
    SetTacticalIntent(ECQBTacticalIntent::HoldAngle, NowSeconds);
    NextPeekTime = NowSeconds + FMath::Max(0.4f, GetConfig()->CombatSettings.BurstCooldownMaxSeconds);
    NextTacticalEvaluationTime = 0.0;
}

void UCQBTrainingCombatComponent::AbandonCover(const double NowSeconds)
{
    GetLocomotion()->Stop();
    bTacticalMovePrepared = false;
    PeekPhase = EPeekPhase::None;
    SelectedCover.bValid = false;
    NextCoverQueryTime = NowSeconds + GetConfig()->TacticalSettings.RepositionCooldownSeconds;
    SetTacticalIntent(ECQBTacticalIntent::DirectFire, NowSeconds);
    NextTacticalEvaluationTime = 0.0;
    if (!GetAwareness()->HasVisibleTarget())
    {
        FinishDefensiveEpisode();
    }
}


