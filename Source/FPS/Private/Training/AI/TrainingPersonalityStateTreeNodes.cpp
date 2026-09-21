#include "Training/AI/TrainingPersonalityStateTreeNodes.h"

#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "Training/AI/TrainingBotPersonalityComponent.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/AI/TrainingCombatAIController.h"

namespace TrainingPersonalityTask
{
    static bool IsUsable(const ATrainingCombatAIController* TrainingController)
    {
        const ATrainingCombatAI* TrainingBot = TrainingController
            ? Cast<ATrainingCombatAI>(TrainingController->GetPawn()) : nullptr;
        return TrainingController && TrainingController->PersonalityComponent && TrainingBot
            && TrainingBot->IsAlive() && TrainingBot->GetTrainingArena();
    }

    static void UpdateMovement(FTrainingBotActionInstanceData& Data,
        ATrainingCombatAIController& TrainingController, bool bForce)
    {
        UTrainingBotPersonalityComponent& Observations = *TrainingController.PersonalityComponent;
        const bool bStrafe = TrainingPersonality::IsStrafeAction(Data.Action);
        if (!Observations.HasParticipant() || Data.Action == ETrainingBotAction::WaitForParticipant
            || Data.Action == ETrainingBotAction::TrickPause)
        {
            if (TrainingController.GetMoveStatus() != EPathFollowingStatus::Idle)
            {
                TrainingController.StopMovement();
            }
            return;
        }

        if (!bForce && Data.RetrySeconds < 0.1f)
        {
            return;
        }
        Data.RetrySeconds = 0.0f;
        TrainingController.RefreshParticipantFocus();
        if (!bForce && !TrainingController.ShouldExtendPersonalityMove())
        {
            return;
        }
        float RadialStep = 0.0f;
        if (Data.Action == ETrainingBotAction::CowardApproach)
        {
            RadialStep = -FMath::Max(0.0f, Observations.ApproachStepCentimeters);
        }
        else if (Data.Action == ETrainingBotAction::CowardEscape)
        {
            RadialStep = FMath::Max(0.0f, Observations.EscapeStepCentimeters);
        }
        if (!TrainingController.RequestPersonalityStep(Data.bClockwise, RadialStep))
        {
            Data.bClockwise = bStrafe
                ? Observations.ReverseStrafeDirection() : !Data.bClockwise;
            if (bStrafe)
            {
                TrainingController.StopMovement();
                TrainingController.RequestPersonalityStep(Data.bClockwise, RadialStep);
            }
        }
    }
}

bool FTrainingBotCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
    const FInstanceDataType& Data = Context.GetInstanceData(*this);
    const ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(Data.AIController);
    if (!TrainingPersonalityTask::IsUsable(TrainingController))
    {
        return false;
    }
    const UTrainingBotPersonalityComponent& Observations = *TrainingController->PersonalityComponent;
    bool bValue = false;
    switch (Data.Test)
    {
    case ETrainingBotCondition::PersonalityIs:
        bValue = Observations.GetActivePersonality() == Data.Personality;
        break;
    case ETrainingBotCondition::HasParticipant:
        bValue = Observations.HasParticipant();
        break;
    case ETrainingBotCondition::IsBeingAimedAt:
        bValue = Observations.IsBeingAimedAt();
        break;
    default:
        return false;
    }
    return bValue == Data.bExpected;
}

FTrainingBotActionTask::FTrainingBotActionTask()
{
    bShouldCallTick = true;
    bShouldCopyBoundPropertiesOnTick = false;
}

EStateTreeRunStatus FTrainingBotActionTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult&) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);
    ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(Data.AIController);
    if (!TrainingPersonalityTask::IsUsable(TrainingController))
    {
        return EStateTreeRunStatus::Failed;
    }
    UTrainingBotPersonalityComponent& Observations = *TrainingController->PersonalityComponent;
    const bool bStrafe = TrainingPersonality::IsStrafeAction(Data.Action);
    const float DurationSeconds = bStrafe ? Observations.StrafeDirectionSeconds : Data.DurationSeconds;
    const float DurationVariance = bStrafe ? Observations.StrafeDirectionVariance : Data.DurationVariance;
    if (!FMath::IsFinite(DurationSeconds) || !FMath::IsFinite(DurationVariance)
        || !FMath::IsFinite(Data.SpeedMultiplier))
    {
        return EStateTreeRunStatus::Failed;
    }
    Data.ElapsedSeconds = 0.0f;
    Data.RetrySeconds = 0.0f;
    const float Variance = FMath::Max(0.0f, DurationVariance);
    Data.SelectedDuration = bStrafe || DurationSeconds > 0.0f
        ? FMath::Max(bStrafe ? 0.1f : 0.05f,
            DurationSeconds + FMath::FRandRange(-Variance, Variance)) : 0.0f;

    if (bStrafe)
    {
        Data.bClockwise = Observations.ReverseStrafeDirection();
    }
    else
    {
        if (Data.Action == ETrainingBotAction::TrickCruise)
        {
            Observations.StartTrickSequence();
        }
        Data.bClockwise = Data.Action == ETrainingBotAction::CowardEscape
            ? Observations.ChooseEscapeClockwise() : Observations.GetTrickClockwise();
        if (Data.Action == ETrainingBotAction::TrickFeint)
        {
            Data.bClockwise = !Data.bClockwise;
        }
    }
    TrainingController->SetTrainingSpeedMultiplier(Data.SpeedMultiplier);
    TrainingController->RefreshParticipantFocus();
    TrainingPersonalityTask::UpdateMovement(Data, *TrainingController, true);
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTrainingBotActionTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);
    ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(Data.AIController);
    if (!TrainingPersonalityTask::IsUsable(TrainingController))
    {
        return EStateTreeRunStatus::Failed;
    }
    if (TrainingController->GetMoveStatus() == EPathFollowingStatus::Moving)
    {
        TrainingController->ValidateCurrentMove();
    }
    Data.ElapsedSeconds += FMath::Max(0.0f, DeltaTime);
    Data.RetrySeconds += FMath::Max(0.0f, DeltaTime);
    if (Data.SelectedDuration > 0.0f && Data.ElapsedSeconds >= Data.SelectedDuration)
    {
        return EStateTreeRunStatus::Succeeded;
    }
    TrainingPersonalityTask::UpdateMovement(Data, *TrainingController, false);
    return EStateTreeRunStatus::Running;
}

void FTrainingBotActionTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
    const FInstanceDataType& Data = Context.GetInstanceData(*this);
    if (ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(Data.AIController))
    {
        TrainingController->StopMovement();
    }
}
