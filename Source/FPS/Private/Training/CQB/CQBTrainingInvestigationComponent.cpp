#include "Training/CQB/CQBTrainingInvestigationComponent.h"

#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Training/CQB/CQBTrainingAIComponent.h"
#include "Training/CQB/CQBTrainingAwarenessComponent.h"
#include "Training/CQB/CQBTrainingLocomotionComponent.h"

UCQBTrainingInvestigationComponent::UCQBTrainingInvestigationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCQBTrainingInvestigationComponent::Configure(
    UCQBTrainingAIComponent& InConfig,
    UCQBTrainingAwarenessComponent& InAwareness,
    UCQBTrainingLocomotionComponent& InLocomotion)
{
    ConfigRef = &InConfig;
    AwarenessRef = &InAwareness;
    LocomotionRef = &InLocomotion;
    ResetRuntimeState();
}

void UCQBTrainingInvestigationComponent::ResetRuntimeState()
{
    StateEnterTime = 0.0;
    ActiveEvidenceRevision = 0;
    bMoveAttempted = false;
    bMovePrepared = false;
    PlannedEvidenceRevision = 0;
    bSearchPlanBuilt = false;
    SearchPoints.Reset();
    SearchCenter = FVector::ZeroVector;
    SearchForward = FVector::ForwardVector;
    SearchPointIndex = 0;
    MoveDeadline = 0.0;
    bInspectingPoint = false;
    InspectionStartTime = 0.0;
    InspectionBaseYaw = 0.0f;
}

void UCQBTrainingInvestigationComponent::EnterInvestigate()
{
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    if (!Config || !Awareness || !Locomotion)
    {
        return;
    }

    StateEnterTime = GetWorld()->GetTimeSeconds();
    ActiveEvidenceRevision = Awareness->GetEvidenceRevision();
    bMoveAttempted = false;
    bMovePrepared = false;
    Locomotion->Stop();
    Locomotion->SetMoveSpeed(Config->PatrolSettings.InvestigateSpeed);
}

void UCQBTrainingInvestigationComponent::TickInvestigate(const float DeltaTime)
{
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    if (!Config || !Awareness || !Locomotion || !Awareness->HasPendingNoise())
    {
        return;
    }

    if (ActiveEvidenceRevision != Awareness->GetEvidenceRevision())
    {
        EnterInvestigate();
    }

    const FCQBPatrolSettings& Settings = Config->PatrolSettings;
    const double Now = GetWorld()->GetTimeSeconds();
    const FVector NoiseLocation = Awareness->GetLastHeardLocation();

    if (Now >= Awareness->GetNoiseExpireTime())
    {
        Awareness->CompleteInvestigation(Settings.SearchDurationSeconds, Now);
        Locomotion->Stop();
        return;
    }

    if (Now < StateEnterTime + FMath::Max(0.0f, Settings.InvestigateLookSeconds))
    {
        Locomotion->TurnToward(
            NoiseLocation, DeltaTime, Settings.LookAroundTurnSpeedDegreesPerSecond);
        return;
    }

    if (!bMoveAttempted)
    {
        bMoveAttempted = true;
        bMovePrepared = Locomotion->PrepareMoveGoal(NoiseLocation, Settings.AcceptanceRadius);
        if (bMovePrepared)
        {
            bMovePrepared = Locomotion->RequestPreparedMove();
        }
    }

    const bool bReached = bMovePrepared
        && Locomotion->TickPreparedMove(DeltaTime, Settings.PatrolTurnSpeedDegreesPerSecond);
    if (!bMovePrepared || bReached)
    {
        Awareness->CompleteInvestigation(Settings.SearchDurationSeconds, Now);
        Locomotion->Stop();
    }
}

void UCQBTrainingInvestigationComponent::ExitInvestigate()
{
    if (UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion())
    {
        Locomotion->Stop();
    }
    bMoveAttempted = false;
    bMovePrepared = false;
}

void UCQBTrainingInvestigationComponent::EnterSearch()
{
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    if (!Config || !Awareness || !Locomotion || !Awareness->HasSearchLocation())
    {
        return;
    }

    ActiveEvidenceRevision = Awareness->GetEvidenceRevision();
    Locomotion->Stop();
    Locomotion->SetMoveSpeed(Config->PatrolSettings.InvestigateSpeed);
    bInspectingPoint = false;

    if (!bSearchPlanBuilt || PlannedEvidenceRevision != ActiveEvidenceRevision)
    {
        BuildSearchPlan();
    }
    MoveToSearchPoint();
}

void UCQBTrainingInvestigationComponent::TickSearch(const float DeltaTime)
{
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    if (!Config || !Awareness || !Locomotion || !Awareness->HasSearchLocation())
    {
        return;
    }

    if (ActiveEvidenceRevision != Awareness->GetEvidenceRevision())
    {
        EnterSearch();
        return;
    }

    const FCQBPatrolSettings& Settings = Config->PatrolSettings;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Awareness->HasThreatKnowledge() || Now >= Awareness->GetSearchExpireTime())
    {
        FinishSearch();
        return;
    }

    if (!bInspectingPoint)
    {
        if (bMovePrepared
            && Locomotion->TickPreparedMove(DeltaTime, Settings.PatrolTurnSpeedDegreesPerSecond))
        {
            BeginPointInspection();
        }
        else if (Now >= MoveDeadline)
        {
            // A blocked route must not consume the entire evidence lifetime.
            ++SearchPointIndex;
            MoveToSearchPoint();
        }
        return;
    }

    const float InspectionSeconds = FMath::Max(0.1f, Settings.SearchScanIntervalSeconds);
    const float Elapsed = static_cast<float>(Now - InspectionStartTime);
    if (Elapsed >= InspectionSeconds)
    {
        ++SearchPointIndex;
        MoveToSearchPoint();
        return;
    }

    const float YawOffset = Settings.SearchScanYawDegrees * 0.5f
        * (Elapsed < InspectionSeconds * 0.5f ? -1.0f : 1.0f);
    Locomotion->TurnTowardYaw(
        InspectionBaseYaw + YawOffset, DeltaTime, Settings.LookAroundTurnSpeedDegreesPerSecond);
}

void UCQBTrainingInvestigationComponent::ExitSearch()
{
    if (UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion())
    {
        Locomotion->Stop();
    }
    bMovePrepared = false;
    bInspectingPoint = false;
}

UCQBTrainingAIComponent* UCQBTrainingInvestigationComponent::GetConfig() const
{
    return ConfigRef.Get();
}

UCQBTrainingAwarenessComponent* UCQBTrainingInvestigationComponent::GetAwareness() const
{
    return AwarenessRef.Get();
}

UCQBTrainingLocomotionComponent* UCQBTrainingInvestigationComponent::GetLocomotion() const
{
    return LocomotionRef.Get();
}

APawn* UCQBTrainingInvestigationComponent::GetControlledPawn() const
{
    const AAIController* Controller = Cast<AAIController>(GetOwner());
    return Controller ? Controller->GetPawn() : nullptr;
}

void UCQBTrainingInvestigationComponent::BuildSearchPlan()
{
    const APawn* Pawn = GetControlledPawn();
    if (!Pawn)
    {
        return;
    }

    const FCQBPatrolSettings& Settings = GetConfig()->PatrolSettings;
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    PlannedEvidenceRevision = Awareness->GetEvidenceRevision();
    bSearchPlanBuilt = true;
    SearchPoints.Reset();
    SearchPointIndex = 0;
    SearchCenter = Awareness->GetSearchLocation();
    SearchForward = (SearchCenter - Pawn->GetActorLocation()).GetSafeNormal2D();
    if (SearchForward.IsNearlyZero())
    {
        SearchForward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    }

    const int32 PointCount = FMath::Clamp(Settings.SearchPointCount, 2, 4);
    const float Radius = FMath::Max(100.0f, Settings.SearchRadius);
    const float MinSpacing = FMath::Max(Settings.AcceptanceRadius * 1.5f, Radius * 0.3f);
    const float MaxProjectionDistance = FMath::Max(50.0f,
        static_cast<float>(Settings.NavigationProjectionExtent.Size2D()));
    const float MaxPathLength = static_cast<float>(FVector::Dist2D(Pawn->GetActorLocation(), SearchCenter)) * 2.0f
        + Radius * 2.0f;

    auto AddReachablePoint = [&](const FVector& Candidate)
    {
        FVector ReachableLocation;
        float PathLength = 0.0f;
        if (!Locomotion->FindReachableLocation(Candidate, ReachableLocation, PathLength,
            MaxProjectionDistance, MaxPathLength))
        {
            return;
        }

        for (const FVector& Existing : SearchPoints)
        {
            if (FVector::DistSquared2D(Existing, ReachableLocation) < FMath::Square(MinSpacing))
            {
                return;
            }
        }
        SearchPoints.Add(ReachableLocation);
    };

    // Probe the remembered area and its adjacent approaches once per evidence revision.
    AddReachablePoint(SearchCenter);
    const float SideOrder = (PlannedEvidenceRevision & 1u) != 0u ? 1.0f : -1.0f;
    const float CandidateAngles[] = { 60.0f, -60.0f, 0.0f, 110.0f, -110.0f, 180.0f, 30.0f, -30.0f };
    for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(CandidateAngles))
        && SearchPoints.Num() < PointCount; ++Index)
    {
        const float CandidateRadius = Index >= 6 ? Radius * 0.5f : Radius;
        const FVector Direction = SearchForward.RotateAngleAxis(
            CandidateAngles[Index] * SideOrder, FVector::UpVector);
        AddReachablePoint(SearchCenter + Direction * CandidateRadius);
    }
}

void UCQBTrainingInvestigationComponent::MoveToSearchPoint()
{
    const FCQBPatrolSettings& Settings = GetConfig()->PatrolSettings;
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    Locomotion->Stop();
    bMovePrepared = false;
    bInspectingPoint = false;

    while (SearchPoints.IsValidIndex(SearchPointIndex))
    {
        const FVector& Point = SearchPoints[SearchPointIndex];
        bMovePrepared = Locomotion->PrepareMoveGoal(Point, Settings.AcceptanceRadius)
            && Locomotion->RequestPreparedMove();
        if (bMovePrepared)
        {
            const float Distance = static_cast<float>(FVector::Dist2D(GetConfig()->GetActorLocation(), Point));
            const float TravelBudget = FMath::Clamp(
                Distance / FMath::Max(50.0f, Settings.InvestigateSpeed) * 2.0f
                    + Settings.SearchScanIntervalSeconds,
                1.0f, FMath::Max(1.0f, Settings.InvestigateTimeoutSeconds));
            MoveDeadline = FMath::Min(Awareness->GetSearchExpireTime(),
                GetWorld()->GetTimeSeconds() + TravelBudget);
            return;
        }
        ++SearchPointIndex;
    }

    FinishSearch();
}

void UCQBTrainingInvestigationComponent::BeginPointInspection()
{
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    Locomotion->Stop();
    bMovePrepared = false;
    bInspectingPoint = true;
    InspectionStartTime = GetWorld()->GetTimeSeconds();

    FVector LookDirection = (SearchPoints[SearchPointIndex] - SearchCenter).GetSafeNormal2D();
    if (LookDirection.IsNearlyZero())
    {
        LookDirection = SearchForward;
    }
    InspectionBaseYaw = LookDirection.Rotation().Yaw;
}

void UCQBTrainingInvestigationComponent::FinishSearch()
{
    GetLocomotion()->Stop();
    GetAwareness()->ClearSearch();
    bMovePrepared = false;
    bInspectingPoint = false;
}
