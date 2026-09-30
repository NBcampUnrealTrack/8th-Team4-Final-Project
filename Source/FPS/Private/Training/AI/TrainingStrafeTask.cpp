#include "Training/AI/TrainingStrafeTask.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/AI/TrainingCombatAIController.h"

FTrainingStrafeTask::FTrainingStrafeTask()
{
    bShouldCallTick = true;
    bShouldCopyBoundPropertiesOnTick = false;
}

EStateTreeRunStatus FTrainingStrafeTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult&) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);
    Data.ElapsedSeconds = 0.0f;
    Data.RetrySeconds = 0.0f;
    ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(Data.AIController);
    if (!TrainingController)
    {
        return EStateTreeRunStatus::Failed;
    }
    TrainingController->RefreshParticipantFocus();
    TrainingController->RequestStrafeStep(Data.bClockwise);
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTrainingStrafeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TrainingStrafeTask_Tick);
    FInstanceDataType& Data = Context.GetInstanceData(*this);
    ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(Data.AIController);
    const ATrainingCombatAI* TrainingPawn = TrainingController
        ? Cast<ATrainingCombatAI>(TrainingController->GetPawn()) : nullptr;
    if (!TrainingPawn || !TrainingPawn->IsAlive())
    {
        return EStateTreeRunStatus::Failed;
    }

    if (TrainingController->GetMoveStatus() == EPathFollowingStatus::Moving)
    {
        TrainingController->ValidateCurrentMove();
    }
    Data.ElapsedSeconds += DeltaTime;
    Data.RetrySeconds += DeltaTime;
    if (Data.ElapsedSeconds >= FMath::Max(0.25f, Data.DirectionDurationSeconds))
    {
        return EStateTreeRunStatus::Succeeded;
    }
    if (Data.RetrySeconds >= 0.25f)
    {
        Data.RetrySeconds = 0.0f;
        TrainingController->RefreshParticipantFocus();
        if (TrainingController->GetMoveStatus() == EPathFollowingStatus::Idle)
        {
            TrainingController->RequestStrafeStep(Data.bClockwise);
        }
    }
    return EStateTreeRunStatus::Running;
}

void FTrainingStrafeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
    const FInstanceDataType& Data = Context.GetInstanceData(*this);
    if (ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(Data.AIController))
    {
        TrainingController->StopMovement();
    }
}
