#include "Training/AI/TrainingCombatAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "StateTree.h"
#include "Training/AI/TrainingBotPersonalityComponent.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/TrainingArena.h"

ATrainingCombatAIController::ATrainingCombatAIController()
{
    bStartAILogicOnPossess = false;
    bAllowStrafe = true;
    PersonalityComponent = CreateDefaultSubobject<UTrainingBotPersonalityComponent>(TEXT("PersonalityComponent"));
    StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
    StateTreeComponent->SetStartLogicAutomatically(false);
    StateTreeComponent->AddTickPrerequisiteComponent(PersonalityComponent);
    BrainComponent = StateTreeComponent;
}

bool ATrainingCombatAIController::HasConfiguredStateTree() const
{
    return IsValid(StrafeStateTree) && StrafeStateTree->IsReadyToRun();
}

bool ATrainingCombatAIController::StartTrainingBrain()
{
    const ATrainingCombatAI* TrainingPawn = Cast<ATrainingCombatAI>(GetPawn());
    if (!HasAuthority() || !TrainingPawn || !TrainingPawn->IsAlive()
        || !TrainingPawn->GetTrainingArena() || !HasConfiguredStateTree())
    {
        return false;
    }
    StateTreeComponent->StopLogic(TEXT("Training initialization"));
    RestoreTrainingSpeed();
    OriginalTrainingSpeed = TrainingPawn->GetCharacterMovement()->MaxWalkSpeed;
    bHasOriginalTrainingSpeed = true;
    PersonalityComponent->BeginTraining();
    StateTreeComponent->SetStateTree(StrafeStateTree);
    RefreshParticipantFocus();
    StateTreeComponent->StartLogic();
    if (!StateTreeComponent->IsRunning())
    {
        StopTrainingBrain();
        return false;
    }
    return true;
}

void ATrainingCombatAIController::StopTrainingBrain()
{
    StateTreeComponent->StopLogic(TEXT("Training stopped"));
    PersonalityComponent->EndTraining();
    RestoreTrainingSpeed();
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
}

void ATrainingCombatAIController::RefreshParticipantFocus()
{
    const ATrainingCombatAI* TrainingPawn = Cast<ATrainingCombatAI>(GetPawn());
    const APlayerController* ParticipantPlayer = TrainingPawn ? TrainingPawn->GetParticipant() : nullptr;
    if (ParticipantPlayer && ParticipantPlayer->GetPawn())
    {
        SetFocus(ParticipantPlayer->GetPawn(), EAIFocusPriority::Gameplay);
    }
    else
    {
        ClearFocus(EAIFocusPriority::Gameplay);
    }
}

bool ATrainingCombatAIController::RequestStrafeStep(const bool bClockwise)
{
    const ATrainingCombatAI* TrainingPawn = Cast<ATrainingCombatAI>(GetPawn());
    const ATrainingArena* Arena = TrainingPawn ? TrainingPawn->GetTrainingArena() : nullptr;
    if (!TrainingPawn || !TrainingPawn->IsAlive() || !Arena)
    {
        return false;
    }
    const FTrainingDistanceBand& Band = TrainingPawn->GetDistanceBand();
    FVector Destination;
    if (!Arena->FindStrafeDestination(TrainingPawn->GetActorLocation(), Band,
        TrainingPawn->GetNavAgentPropertiesRef(), bClockwise, Destination))
    {
        return false;
    }

    return RequestTrainingDestination(Destination);
}

bool ATrainingCombatAIController::RequestPersonalityStep(const bool bClockwise, const float RadialStepCentimeters)
{
    const ATrainingCombatAI* TrainingPawn = Cast<ATrainingCombatAI>(GetPawn());
    const ATrainingArena* Arena = TrainingPawn ? TrainingPawn->GetTrainingArena() : nullptr;
    if (!HasAuthority() || !TrainingPawn || !TrainingPawn->IsAlive() || !Arena)
    {
        return false;
    }
    FVector Destination;
    if (!Arena->FindBotStepDestination(TrainingPawn->GetActorLocation(), TrainingPawn->GetDistanceBand(),
        TrainingPawn->GetNavAgentPropertiesRef(), bClockwise ? 150.0f : -150.0f,
        RadialStepCentimeters, Destination))
    {
        return false;
    }
    return RequestTrainingDestination(Destination);
}

bool ATrainingCombatAIController::RequestTrainingDestination(const FVector& Destination)
{
    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalLocation(Destination);
    MoveRequest.SetAcceptanceRadius(12.0f);
    MoveRequest.SetReachTestIncludesAgentRadius(false);
    MoveRequest.SetAllowPartialPath(false);
    MoveRequest.SetProjectGoalLocation(false);
    MoveRequest.SetUsePathfinding(true);
    MoveRequest.SetCanStrafe(true);
    const FPathFollowingRequestResult RequestResult = MoveTo(MoveRequest);
    if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        return true;
    }
    return RequestResult.Code == EPathFollowingRequestResult::RequestSuccessful
        && ValidateCurrentMove();
}

bool ATrainingCombatAIController::ValidateCurrentMove()
{
    const ATrainingCombatAI* TrainingPawn = Cast<ATrainingCombatAI>(GetPawn());
    const ATrainingArena* Arena = TrainingPawn ? TrainingPawn->GetTrainingArena() : nullptr;
    const UPathFollowingComponent* PathFollower = GetPathFollowingComponent();
    if (!TrainingPawn || !TrainingPawn->IsAlive() || !Arena || !PathFollower)
    {
        StopMovement();
        return false;
    }
    const FNavPathSharedPtr CurrentPath = PathFollower->GetPath();
    if (!CurrentPath.IsValid())
    {
        StopMovement();
        return false;
    }

    const TArray<FNavPathPoint>& PathPoints = CurrentPath->GetPathPoints();
    const int32 NextPointIndex = static_cast<int32>(PathFollower->GetNextPathIndex());
    if (!PathPoints.IsValidIndex(NextPointIndex))
    {
        StopMovement();
        return false;
    }

    const FTrainingDistanceBand& Band = TrainingPawn->GetDistanceBand();
    const FVector Center = Arena->GetCenterLocation();
    FVector PreviousPoint = TrainingPawn->GetActorLocation();
    for (int32 PointIndex = NextPointIndex; PointIndex < PathPoints.Num(); ++PointIndex)
    {
        const FVector& NextPoint = PathPoints[PointIndex].Location;
        if (!Band.ContainsSegment(Center, PreviousPoint, NextPoint))
        {
            StopMovement();
            return false;
        }
        PreviousPoint = NextPoint;
    }
    return true;
}

void ATrainingCombatAIController::OnUnPossess()
{
    StopTrainingBrain();
    Super::OnUnPossess();
}

void ATrainingCombatAIController::SetTrainingSpeedMultiplier(const float Multiplier)
{
    ATrainingCombatAI* TrainingPawn = Cast<ATrainingCombatAI>(GetPawn());
    if (TrainingPawn && bHasOriginalTrainingSpeed && FMath::IsFinite(Multiplier))
    {
        TrainingPawn->GetCharacterMovement()->MaxWalkSpeed =
            OriginalTrainingSpeed * FMath::Clamp(Multiplier, 0.1f, 4.0f);
    }
}

void ATrainingCombatAIController::RestoreTrainingSpeed()
{
    ATrainingCombatAI* TrainingPawn = Cast<ATrainingCombatAI>(GetPawn());
    if (TrainingPawn && bHasOriginalTrainingSpeed)
    {
        TrainingPawn->GetCharacterMovement()->MaxWalkSpeed = OriginalTrainingSpeed;
    }
    bHasOriginalTrainingSpeed = false;
}

bool ATrainingCombatAIController::ShouldExtendPersonalityMove() const
{
    if (GetMoveStatus() == EPathFollowingStatus::Idle)
    {
        return true;
    }
    const APawn* ControlledPawn = GetPawn();
    const UPathFollowingComponent* PathFollower = GetPathFollowingComponent();
    const FNavPathSharedPtr Path = PathFollower ? PathFollower->GetPath() : FNavPathSharedPtr();
    if (!ControlledPawn || !Path.IsValid() || Path->GetPathPoints().IsEmpty())
    {
        return false;
    }
    return FVector::DistSquared2D(ControlledPawn->GetActorLocation(), Path->GetPathPoints().Last().Location)
        <= FMath::Square(65.0);
}
