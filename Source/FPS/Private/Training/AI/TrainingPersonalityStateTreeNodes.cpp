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
        return TrainingController && TrainingController->HasAuthority()
            && TrainingController->PersonalityComponent && TrainingBot
            && TrainingBot->IsAlive() && TrainingBot->GetTrainingArena();
    }

    static void UpdateMovement(FTrainingBotActionInstanceData& Data,
        ATrainingCombatAIController& TrainingController, bool bForce)
    {
        const UTrainingBotPersonalityComponent& Observations = *TrainingController.PersonalityComponent;
        const bool bCopy = TrainingPersonality::IsCopyAction(Data.Action);
        if (!Observations.HasParticipant() || Data.Action == ETrainingBotAction::WaitForParticipant
            || Data.Action == ETrainingBotAction::TrickPause)
        {
            if (TrainingController.GetMoveStatus() != EPathFollowingStatus::Idle)
            {
                TrainingController.StopMovement();
            }
            Data.LastCopyDirection = 0;
            return;
        }

        if (bCopy)
        {
            float SignedSpeed = Observations.GetCopyTangentialVelocity(Data.Action == ETrainingBotAction::CopyDelayed);
            if (Data.Action == ETrainingBotAction::CopyInverse)
            {
                SignedSpeed = -SignedSpeed;
            }
            const int32 Direction = FMath::Abs(SignedSpeed) < FMath::Max(1.0f, Observations.CopyMovementDeadZone)
                ? 0 : (SignedSpeed > 0.0f ? 1 : -1);
            if (Direction == 0)
            {
                if (TrainingController.GetMoveStatus() != EPathFollowingStatus::Idle)
                {
                    TrainingController.StopMovement();
                }
                Data.LastCopyDirection = 0;
                return;
            }
            if (Direction != Data.LastCopyDirection)
            {
                TrainingController.StopMovement();
                Data.bClockwise = Direction > 0;
                Data.LastCopyDirection = Direction;
                bForce = true;
            }
            TrainingController.SetTrainingSpeedMultiplier(Data.SpeedMultiplier
                * FMath::Clamp(FMath::Abs(SignedSpeed) / 250.0f, 0.4f, 1.3f));
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
            RadialStep = -65.0f;
        }
        else if (Data.Action == ETrainingBotAction::CowardEscape)
        {
            RadialStep = 100.0f;
        }
        if (!TrainingController.RequestPersonalityStep(Data.bClockwise, RadialStep) && !bCopy)
        {
            Data.bClockwise = !Data.bClockwise;
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
    if (!FMath::IsFinite(Data.DurationSeconds) || !FMath::IsFinite(Data.DurationVariance)
        || !FMath::IsFinite(Data.SpeedMultiplier))
    {
        return EStateTreeRunStatus::Failed;
    }
    Data.ElapsedSeconds = 0.0f;
    Data.RetrySeconds = 0.0f;
    Data.LastCopyDirection = 0;
    const float Variance = FMath::Max(0.0f, Data.DurationVariance);
    Data.SelectedDuration = Data.DurationSeconds > 0.0f
        ? FMath::Max(0.05f, Data.DurationSeconds + FMath::FRandRange(-Variance, Variance)) : 0.0f;

    UTrainingBotPersonalityComponent& Observations = *TrainingController->PersonalityComponent;
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
