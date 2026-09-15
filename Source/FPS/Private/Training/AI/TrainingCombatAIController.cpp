#include "Training/AI/TrainingCombatAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "GameFramework/PlayerController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "StateTree.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/TrainingArena.h"

ATrainingCombatAIController::ATrainingCombatAIController()
{
    bStartAILogicOnPossess = false;
    bAllowStrafe = true;
    StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
    StateTreeComponent->SetStartLogicAutomatically(false);
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
    StateTreeComponent->SetStateTree(StrafeStateTree);
    RefreshParticipantFocus();
    StateTreeComponent->StartLogic();
    return StateTreeComponent->IsRunning();
}

void ATrainingCombatAIController::StopTrainingBrain()
{
    StateTreeComponent->StopLogic(TEXT("Training stopped"));
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
        // A missing/replaced player Pawn does not terminate this prototype's session.
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

    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalLocation(Destination);
    MoveRequest.SetAcceptanceRadius(12.0f);
    MoveRequest.SetReachTestIncludesAgentRadius(false);
    MoveRequest.SetAllowPartialPath(false);
    MoveRequest.SetProjectGoalLocation(false);
    MoveRequest.SetUsePathfinding(true);
    MoveRequest.SetCanStrafe(true);
    const FPathFollowingRequestResult RequestResult = MoveTo(MoveRequest);
    if (RequestResult.Code == EPathFollowingRequestResult::Failed)
    {
        return false;
    }
    return ValidateCurrentMove();
}

bool ATrainingCombatAIController::ValidateCurrentMove()
{
    const ATrainingCombatAI* TrainingPawn = Cast<ATrainingCombatAI>(GetPawn());
    const ATrainingArena* Arena = TrainingPawn ? TrainingPawn->GetTrainingArena() : nullptr;
    const FNavPathSharedPtr CurrentPath = GetPathFollowingComponent()->GetPath();
    if (!TrainingPawn || !Arena || !CurrentPath.IsValid())
    {
        return false;
    }
    const FTrainingDistanceBand& Band = TrainingPawn->GetDistanceBand();
    FVector PreviousPoint = TrainingPawn->GetActorLocation();
    for (const FNavPathPoint& PathPoint : CurrentPath->GetPathPoints())
    {
        if (!Band.ContainsSegment(Arena->GetCenterLocation(), PreviousPoint, PathPoint.Location))
        {
            StopMovement();
            return false;
        }
        PreviousPoint = PathPoint.Location;
    }
    return true;
}

void ATrainingCombatAIController::OnUnPossess()
{
    StopTrainingBrain();
    Super::OnUnPossess();
}
