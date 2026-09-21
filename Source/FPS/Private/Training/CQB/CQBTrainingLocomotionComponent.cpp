#include "Training/CQB/CQBTrainingLocomotionComponent.h"

#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogCQBTrainingLocomotion, Log, All);

UCQBTrainingLocomotionComponent::UCQBTrainingLocomotionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCQBTrainingLocomotionComponent::Configure(const FCQBPatrolSettings& InSettings)
{
    Settings = InSettings;
    ResetRuntimeState();
}

void UCQBTrainingLocomotionComponent::ResetRuntimeState()
{
    MoveGoal = FVector::ZeroVector;
    MoveAcceptanceRadius = FMath::Max(10.0f, Settings.AcceptanceRadius);
    bHasMoveGoal = false;
    bMoveRequested = false;
    NextMoveRetryTime = 0.0;
    NextMoveFailureLogTime = 0.0;
}

void UCQBTrainingLocomotionComponent::Stop()
{
    if (AAIController* Controller = GetController())
    {
        Controller->StopMovement();
        Controller->ClearFocus(EAIFocusPriority::Gameplay);
        Controller->ClearFocus(EAIFocusPriority::Move);
    }

    bMoveRequested = false;
    bHasMoveGoal = false;
    MoveGoal = FVector::ZeroVector;
    NextMoveRetryTime = 0.0;
}

void UCQBTrainingLocomotionComponent::SetMoveSpeed(const float Speed) const
{
    if (APawn* Pawn = GetControlledPawn())
    {
        if (UCharacterMovementComponent* Movement = Pawn->FindComponentByClass<UCharacterMovementComponent>())
        {
            Movement->MaxWalkSpeed = FMath::Max(50.0f, Speed);
        }
    }
}

bool UCQBTrainingLocomotionComponent::PrepareMoveGoal(
    const FVector& Destination,
    const float AcceptanceRadius)
{
    bMoveRequested = false;
    bHasMoveGoal = ResolveNavigationGoal(Destination, MoveGoal);
    MoveAcceptanceRadius = FMath::Max(10.0f, AcceptanceRadius);
    NextMoveRetryTime = 0.0;
    return bHasMoveGoal;
}

bool UCQBTrainingLocomotionComponent::RequestPreparedMove()
{
    AAIController* Controller = GetController();
    UWorld* World = GetWorld();
    if (!Controller || !World || !bHasMoveGoal)
    {
        return false;
    }

    if (HasReachedPreparedGoal())
    {
        bMoveRequested = false;
        return true;
    }

    if (World->GetTimeSeconds() < NextMoveRetryTime)
    {
        return false;
    }

    FAIMoveRequest Request;
    Request.SetGoalLocation(MoveGoal);
    Request.SetAcceptanceRadius(MoveAcceptanceRadius);
    Request.SetReachTestIncludesAgentRadius(false);
    Request.SetAllowPartialPath(false);
    Request.SetProjectGoalLocation(false);
    Request.SetUsePathfinding(true);
    Request.SetCanStrafe(false);

    const FPathFollowingRequestResult Result = Controller->MoveTo(Request);
    const bool bAccepted = Result.Code == EPathFollowingRequestResult::RequestSuccessful
        || Result.Code == EPathFollowingRequestResult::AlreadyAtGoal;

    bMoveRequested = Result.Code == EPathFollowingRequestResult::RequestSuccessful;
    if (!bAccepted)
    {
        LogMoveFailure(TEXT("MoveTo failed"), MoveGoal, static_cast<int32>(Result.Code));
        ScheduleMoveRetry();
    }
    return bAccepted;
}

bool UCQBTrainingLocomotionComponent::TickPreparedMove(
    const float DeltaTime,
    const float TurnSpeedDegreesPerSecond)
{
    AAIController* Controller = GetController();
    UWorld* World = GetWorld();
    if (!Controller || !World || !bHasMoveGoal)
    {
        return false;
    }

    if (HasReachedPreparedGoal())
    {
        Controller->StopMovement();
        bMoveRequested = false;
        return true;
    }

    UpdateMoveFacing(DeltaTime, TurnSpeedDegreesPerSecond);

    if (bMoveRequested && Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
    {
        bMoveRequested = false;
        ScheduleMoveRetry();
    }

    if (!bMoveRequested && World->GetTimeSeconds() >= NextMoveRetryTime)
    {
        RequestPreparedMove();
    }

    return HasReachedPreparedGoal();
}

bool UCQBTrainingLocomotionComponent::HasReachedPreparedGoal() const
{
    const APawn* Pawn = GetControlledPawn();
    return bHasMoveGoal
        && Pawn
        && FVector::DistSquared2D(Pawn->GetActorLocation(), MoveGoal)
            <= FMath::Square(MoveAcceptanceRadius);
}

void UCQBTrainingLocomotionComponent::UpdateMoveFacing(
    const float DeltaTime,
    const float TurnSpeedDegreesPerSecond)
{
    const APawn* Pawn = GetControlledPawn();
    if (!Pawn || !bHasMoveGoal)
    {
        return;
    }

    FVector FacingPoint = MoveGoal;
    const FVector Velocity = Pawn->GetVelocity();
    if (Velocity.SizeSquared2D() > FMath::Square(10.0f))
    {
        FacingPoint = Pawn->GetActorLocation() + Velocity.GetSafeNormal2D() * 500.0f;
    }

    TurnToward(FacingPoint, DeltaTime, TurnSpeedDegreesPerSecond, false);
}

bool UCQBTrainingLocomotionComponent::TurnToward(
    const FVector& WorldLocation,
    const float DeltaTime,
    const float TurnSpeedDegreesPerSecond,
    const bool bAimPitch)
{
    AAIController* Controller = GetController();
    APawn* Pawn = GetControlledPawn();
    if (!Controller || !Pawn)
    {
        return false;
    }

    FVector FlatDirection = WorldLocation - Pawn->GetActorLocation();
    FlatDirection.Z = 0.0f;
    if (FlatDirection.IsNearlyZero())
    {
        return true;
    }

    const float TargetYaw = FlatDirection.Rotation().Yaw;
    FRotator ActorRotation = Pawn->GetActorRotation();
    const float MaxYawStep = FMath::Max(0.0f, TurnSpeedDegreesPerSecond)
        * FMath::Max(0.0f, DeltaTime);
    ActorRotation.Pitch = 0.0f;
    ActorRotation.Roll = 0.0f;
    ActorRotation.Yaw = FMath::FixedTurn(ActorRotation.Yaw, TargetYaw, MaxYawStep);
    Pawn->SetActorRotation(ActorRotation);

    FRotator ControlRotation = ActorRotation;
    if (bAimPitch)
    {
        const FVector AimDirection = WorldLocation - Pawn->GetPawnViewLocation();
        if (!AimDirection.IsNearlyZero())
        {
            ControlRotation.Pitch = AimDirection.Rotation().Pitch;
        }
    }
    else
    {
        ControlRotation.Pitch = 0.0f;
    }
    ControlRotation.Yaw = ActorRotation.Yaw;
    ControlRotation.Roll = 0.0f;
    Controller->SetControlRotation(ControlRotation);

    return FMath::Abs(FMath::FindDeltaAngleDegrees(ActorRotation.Yaw, TargetYaw)) <= 0.5f;
}

bool UCQBTrainingLocomotionComponent::TurnTowardYaw(
    const float TargetYaw,
    const float DeltaTime,
    const float TurnSpeedDegreesPerSecond)
{
    AAIController* Controller = GetController();
    APawn* Pawn = GetControlledPawn();
    if (!Controller || !Pawn)
    {
        return false;
    }

    FRotator ActorRotation = Pawn->GetActorRotation();
    const float MaxYawStep = FMath::Max(0.0f, TurnSpeedDegreesPerSecond)
        * FMath::Max(0.0f, DeltaTime);
    ActorRotation.Pitch = 0.0f;
    ActorRotation.Roll = 0.0f;
    ActorRotation.Yaw = FMath::FixedTurn(ActorRotation.Yaw, TargetYaw, MaxYawStep);
    Pawn->SetActorRotation(ActorRotation);

    FRotator ControlRotation = Controller->GetControlRotation();
    ControlRotation.Pitch = 0.0f;
    ControlRotation.Roll = 0.0f;
    ControlRotation.Yaw = ActorRotation.Yaw;
    Controller->SetControlRotation(ControlRotation);

    return FMath::Abs(FMath::FindDeltaAngleDegrees(ActorRotation.Yaw, TargetYaw)) <= 0.5f;
}

bool UCQBTrainingLocomotionComponent::IsFacing(
    const FVector& WorldLocation,
    const float ToleranceDegrees) const
{
    const APawn* Pawn = GetControlledPawn();
    if (!Pawn)
    {
        return false;
    }

    FVector FlatDirection = WorldLocation - Pawn->GetActorLocation();
    FlatDirection.Z = 0.0f;
    if (FlatDirection.IsNearlyZero())
    {
        return true;
    }

    const float TargetYaw = FlatDirection.Rotation().Yaw;
    const float Error = FMath::Abs(FMath::FindDeltaAngleDegrees(Pawn->GetActorRotation().Yaw, TargetYaw));
    return Error <= FMath::Max(0.5f, ToleranceDegrees);
}

AAIController* UCQBTrainingLocomotionComponent::GetController() const
{
    return Cast<AAIController>(GetOwner());
}

APawn* UCQBTrainingLocomotionComponent::GetControlledPawn() const
{
    const AAIController* Controller = GetController();
    return Controller ? Controller->GetPawn() : nullptr;
}

bool UCQBTrainingLocomotionComponent::ResolveNavigationGoal(
    const FVector& Destination,
    FVector& OutResolvedGoal) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSystem)
    {
        LogMoveFailure(TEXT("Navigation system unavailable"), Destination);
        return false;
    }

    FNavLocation ProjectedLocation;
    if (!NavSystem->ProjectPointToNavigation(
        Destination, ProjectedLocation, Settings.NavigationProjectionExtent.GetAbs()))
    {
        LogMoveFailure(TEXT("NavMesh projection failed"), Destination);
        return false;
    }

    OutResolvedGoal = ProjectedLocation.Location;
    return true;
}

void UCQBTrainingLocomotionComponent::ScheduleMoveRetry()
{
    if (const UWorld* World = GetWorld())
    {
        NextMoveRetryTime = World->GetTimeSeconds()
            + FMath::Max(0.05f, Settings.MoveRetryDelaySeconds);
    }
}

void UCQBTrainingLocomotionComponent::LogMoveFailure(
    const TCHAR* Reason,
    const FVector& Goal,
    const int32 ResultCode) const
{
    const UWorld* World = GetWorld();
    if (!World || World->GetTimeSeconds() < NextMoveFailureLogTime)
    {
        return;
    }

    if (ResultCode == INDEX_NONE)
    {
        UE_LOG(LogCQBTrainingLocomotion, Warning,
            TEXT("%s: %s. Goal=%s"),
            *GetNameSafe(GetControlledPawn()), Reason, *Goal.ToString());
    }
    else
    {
        UE_LOG(LogCQBTrainingLocomotion, Warning,
            TEXT("%s: %s. Goal=%s Result=%d"),
            *GetNameSafe(GetControlledPawn()), Reason, *Goal.ToString(), ResultCode);
    }

    NextMoveFailureLogTime = World->GetTimeSeconds()
        + FMath::Max(0.1f, Settings.MoveFailureLogIntervalSeconds);
}
