#include "Training/CQB/CQBTrainingInvestigationComponent.h"

#include "AIController.h"
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
    bMovePrepared = false;
    bSearchScanStarted = false;
    SearchScanStep = 0;
    NextSearchScanTime = 0.0;
    SearchBaseYaw = 0.0f;
    SearchLookTargetYaw = 0.0f;
}

void UCQBTrainingInvestigationComponent::EnterInvestigate()
{
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    UWorld* World = GetWorld();
    if (!Config || !Locomotion || !World)
    {
        return;
    }

    ResetRuntimeState();
    StateEnterTime = World->GetTimeSeconds();
    Locomotion->Stop();
    Locomotion->SetMoveSpeed(Config->PatrolSettings.InvestigateSpeed);
}

void UCQBTrainingInvestigationComponent::TickInvestigate(const float DeltaTime)
{
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    UWorld* World = GetWorld();
    if (!Config || !Awareness || !Locomotion || !World || !Awareness->HasPendingNoise())
    {
        return;
    }

    const FCQBPatrolSettings& Settings = Config->PatrolSettings;
    const double Now = World->GetTimeSeconds();
    const FVector NoiseLocation = Awareness->GetLastHeardLocation();

    if (Now < StateEnterTime + FMath::Max(0.0f, Settings.InvestigateLookSeconds))
    {
        Locomotion->TurnToward(
            NoiseLocation, DeltaTime, Settings.LookAroundTurnSpeedDegreesPerSecond);
        return;
    }

    if (!bMovePrepared)
    {
        bMovePrepared = Locomotion->PrepareMoveGoal(NoiseLocation, Settings.AcceptanceRadius);
        if (bMovePrepared)
        {
            Locomotion->RequestPreparedMove();
        }
    }

    const bool bReached = bMovePrepared
        && Locomotion->TickPreparedMove(DeltaTime, Settings.PatrolTurnSpeedDegreesPerSecond);
    if (bReached || Now >= Awareness->GetNoiseExpireTime())
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
    bMovePrepared = false;
}

void UCQBTrainingInvestigationComponent::EnterSearch()
{
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    UWorld* World = GetWorld();
    if (!Config || !Awareness || !Locomotion || !World || !Awareness->HasSearchLocation())
    {
        return;
    }

    ResetRuntimeState();
    StateEnterTime = World->GetTimeSeconds();
    Locomotion->Stop();
    Locomotion->SetMoveSpeed(Config->PatrolSettings.InvestigateSpeed);
    bMovePrepared = Locomotion->PrepareMoveGoal(
        Awareness->GetSearchLocation(), Config->PatrolSettings.AcceptanceRadius);
    if (bMovePrepared)
    {
        Locomotion->RequestPreparedMove();
    }
}

void UCQBTrainingInvestigationComponent::TickSearch(const float DeltaTime)
{
    UCQBTrainingAIComponent* Config = GetConfig();
    UCQBTrainingAwarenessComponent* Awareness = GetAwareness();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    APawn* Pawn = GetControlledPawn();
    UWorld* World = GetWorld();
    if (!Config || !Awareness || !Locomotion || !Pawn || !World || !Awareness->HasSearchLocation())
    {
        return;
    }

    const FCQBPatrolSettings& Settings = Config->PatrolSettings;
    const double Now = World->GetTimeSeconds();
    if (Now >= Awareness->GetSearchExpireTime())
    {
        Awareness->ClearSearch();
        Locomotion->Stop();
        return;
    }

    if (!bSearchScanStarted)
    {
        if (!bMovePrepared)
        {
            bMovePrepared = Locomotion->PrepareMoveGoal(
                Awareness->GetSearchLocation(), Settings.AcceptanceRadius);
        }

        if (!bMovePrepared
            || !Locomotion->TickPreparedMove(DeltaTime, Settings.PatrolTurnSpeedDegreesPerSecond))
        {
            return;
        }

        BeginSearchScan();
    }

    if (Now >= NextSearchScanTime)
    {
        const float YawOffset = SearchScanStep % 3 == 0
            ? -Settings.SearchScanYawDegrees
            : SearchScanStep % 3 == 1
                ? Settings.SearchScanYawDegrees
                : 0.0f;
        SearchLookTargetYaw = SearchBaseYaw + YawOffset;
        ++SearchScanStep;
        NextSearchScanTime = Now + FMath::Max(0.1f, Settings.SearchScanIntervalSeconds);
    }

    Locomotion->TurnTowardYaw(
        SearchLookTargetYaw, DeltaTime, Settings.LookAroundTurnSpeedDegreesPerSecond);
}

void UCQBTrainingInvestigationComponent::ExitSearch()
{
    if (UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion())
    {
        Locomotion->Stop();
    }
    bMovePrepared = false;
    bSearchScanStarted = false;
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

void UCQBTrainingInvestigationComponent::BeginSearchScan()
{
    APawn* Pawn = GetControlledPawn();
    UWorld* World = GetWorld();
    UCQBTrainingLocomotionComponent* Locomotion = GetLocomotion();
    if (!Pawn || !World || !Locomotion)
    {
        return;
    }

    Locomotion->Stop();
    bSearchScanStarted = true;
    SearchScanStep = 0;
    SearchBaseYaw = Pawn->GetActorRotation().Yaw;
    SearchLookTargetYaw = SearchBaseYaw;
    NextSearchScanTime = World->GetTimeSeconds();
}
