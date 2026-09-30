#include "Training/CQB/CQBTrainingStateTreeNodes.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"
#include "Training/CQB/CQBTrainingAIComponent.h"
#include "Training/CQB/CQBTrainingAwarenessComponent.h"
#include "Training/CQB/CQBTrainingCombatComponent.h"
#include "Training/CQB/CQBTrainingInvestigationComponent.h"
#include "Training/CQB/CQBTrainingPatrolComponent.h"

namespace CQBStateTree
{
    static ACQBTrainingAIController* ResolveController(FStateTreeExecutionContext& Context)
    {
        UObject* Owner = Context.GetOwner();
        if (ACQBTrainingAIController* Controller = Cast<ACQBTrainingAIController>(Owner))
        {
            return Controller;
        }
        if (const UActorComponent* Component = Cast<UActorComponent>(Owner))
        {
            return Cast<ACQBTrainingAIController>(Component->GetOwner());
        }
        return nullptr;
    }

    static bool IsStateSelected(
        const ACQBTrainingAIController& Controller,
        const ECQBAIBehaviorState State)
    {
        const APawn* Pawn = Controller.GetPawn();
        const UCQBTrainingAIComponent* Config = Pawn
            ? Pawn->FindComponentByClass<UCQBTrainingAIComponent>()
            : nullptr;
        const UCQBTrainingAwarenessComponent* Awareness = Controller.GetAwarenessComponent();
        const UWorld* World = Controller.GetWorld();
        if (!Config || !Config->IsAlive() || !Awareness || !World)
        {
            return false;
        }

        const double Now = World->GetTimeSeconds();
        const bool bValidTarget = Controller.IsCombatTargetValid(Awareness->GetTargetActor());
        const bool bVisibleTarget = Awareness->HasVisibleTarget() && bValidTarget;
        if (bVisibleTarget)
        {
            const bool bReady = Controller.GetCombatComponent()->IsTacticalEngagementActive()
                || Awareness->IsReactionReady(Now);
            return State == (bReady ? ECQBAIBehaviorState::Engage : ECQBAIBehaviorState::Reaction);
        }
        if (Controller.GetCombatComponent()->HasTacticalOpportunity())
        {
            return State == ECQBAIBehaviorState::Engage;
        }
        if (Awareness->HasPendingNoise())
        {
            return State == ECQBAIBehaviorState::Investigate;
        }
        if (Awareness->HasSearchLocation())
        {
            return State == ECQBAIBehaviorState::Search;
        }
        return State == ECQBAIBehaviorState::Patrol;
    }

    static void EnterBehavior(ACQBTrainingAIController& Controller, const ECQBAIBehaviorState State)
    {
        switch (State)
        {
        case ECQBAIBehaviorState::Patrol:
            if (UCQBTrainingPatrolComponent* Patrol = Controller.GetPatrolComponent())
            {
                Patrol->EnterPatrol();
            }
            break;
        case ECQBAIBehaviorState::Investigate:
            if (UCQBTrainingInvestigationComponent* Investigation = Controller.GetInvestigationComponent())
            {
                Investigation->EnterInvestigate();
            }
            break;
        case ECQBAIBehaviorState::Reaction:
            if (UCQBTrainingCombatComponent* Combat = Controller.GetCombatComponent())
            {
                Combat->EnterReaction();
            }
            break;
        case ECQBAIBehaviorState::Engage:
            if (UCQBTrainingCombatComponent* Combat = Controller.GetCombatComponent())
            {
                Combat->EnterEngage();
            }
            break;
        case ECQBAIBehaviorState::Search:
            if (UCQBTrainingInvestigationComponent* Investigation = Controller.GetInvestigationComponent())
            {
                Investigation->EnterSearch();
            }
            break;
        }
    }

    static void TickBehavior(
        ACQBTrainingAIController& Controller,
        const ECQBAIBehaviorState State,
        const float DeltaTime)
    {
        switch (State)
        {
        case ECQBAIBehaviorState::Patrol:
            if (UCQBTrainingPatrolComponent* Patrol = Controller.GetPatrolComponent())
            {
                Patrol->TickPatrol(DeltaTime);
            }
            break;
        case ECQBAIBehaviorState::Investigate:
            if (UCQBTrainingInvestigationComponent* Investigation = Controller.GetInvestigationComponent())
            {
                Investigation->TickInvestigate(DeltaTime);
            }
            break;
        case ECQBAIBehaviorState::Reaction:
            if (UCQBTrainingCombatComponent* Combat = Controller.GetCombatComponent())
            {
                Combat->TickReaction(DeltaTime);
            }
            break;
        case ECQBAIBehaviorState::Engage:
            if (UCQBTrainingCombatComponent* Combat = Controller.GetCombatComponent())
            {
                Combat->TickEngage(DeltaTime);
            }
            break;
        case ECQBAIBehaviorState::Search:
            if (UCQBTrainingInvestigationComponent* Investigation = Controller.GetInvestigationComponent())
            {
                Investigation->TickSearch(DeltaTime);
            }
            break;
        }
    }

    static void ExitBehavior(ACQBTrainingAIController& Controller, const ECQBAIBehaviorState State)
    {
        switch (State)
        {
        case ECQBAIBehaviorState::Patrol:
            if (UCQBTrainingPatrolComponent* Patrol = Controller.GetPatrolComponent())
            {
                Patrol->ExitPatrol();
            }
            break;
        case ECQBAIBehaviorState::Investigate:
            if (UCQBTrainingInvestigationComponent* Investigation = Controller.GetInvestigationComponent())
            {
                Investigation->ExitInvestigate();
            }
            break;
        case ECQBAIBehaviorState::Reaction:
        case ECQBAIBehaviorState::Engage:
            if (UCQBTrainingCombatComponent* Combat = Controller.GetCombatComponent())
            {
                Combat->ExitCombat();
            }
            break;
        case ECQBAIBehaviorState::Search:
            if (UCQBTrainingInvestigationComponent* Investigation = Controller.GetInvestigationComponent())
            {
                Investigation->ExitSearch();
            }
            break;
        }
    }
}

bool FCQBBehaviorStateCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
    const FInstanceDataType& Data = Context.GetInstanceData(*this);
    const ACQBTrainingAIController* Controller = CQBStateTree::ResolveController(Context);
    if (!Controller)
    {
        return false;
    }

    return CQBStateTree::IsStateSelected(*Controller, Data.DesiredState) == Data.bExpected;
}

FCQBBehaviorStateTask::FCQBBehaviorStateTask()
{
    bShouldCallTick = true;
    bShouldCopyBoundPropertiesOnTick = false;
    bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FCQBBehaviorStateTask::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult&) const
{
    const FInstanceDataType& Data = Context.GetInstanceData(*this);
    ACQBTrainingAIController* Controller = CQBStateTree::ResolveController(Context);
    if (!Controller || !CQBStateTree::IsStateSelected(*Controller, Data.State))
    {
        return EStateTreeRunStatus::Failed;
    }

    CQBStateTree::EnterBehavior(*Controller, Data.State);
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FCQBBehaviorStateTask::Tick(
    FStateTreeExecutionContext& Context,
    const float DeltaTime) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CQBBehaviorStateTask_Tick);
    const FInstanceDataType& Data = Context.GetInstanceData(*this);
    ACQBTrainingAIController* Controller = CQBStateTree::ResolveController(Context);
    if (!Controller)
    {
        return EStateTreeRunStatus::Failed;
    }
    if (!CQBStateTree::IsStateSelected(*Controller, Data.State))
    {
        return EStateTreeRunStatus::Succeeded;
    }

    if (Data.State == ECQBAIBehaviorState::Engage && Data.bUseTacticalChildren)
    {
        Controller->GetCombatComponent()->EvaluateTacticalIntent();
    }
    else
    {
        CQBStateTree::TickBehavior(*Controller, Data.State, DeltaTime);
    }
    return CQBStateTree::IsStateSelected(*Controller, Data.State)
        ? EStateTreeRunStatus::Running
        : EStateTreeRunStatus::Succeeded;
}

bool FCQBTacticalIntentCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
    const ACQBTrainingAIController* Controller = CQBStateTree::ResolveController(Context);
    const FInstanceDataType& Data = Context.GetInstanceData(*this);
    return Controller && Controller->GetCombatComponent()->GetTacticalIntent() == Data.Intent;
}

FCQBTacticalIntentTask::FCQBTacticalIntentTask()
{
    bShouldCallTick = true;
    bShouldCopyBoundPropertiesOnTick = false;
    bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FCQBTacticalIntentTask::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult&) const
{
    // The Engage parent initializes combat after initial child selection.
    return CQBStateTree::ResolveController(Context)
        ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCQBTacticalIntentTask::Tick(
    FStateTreeExecutionContext& Context,
    const float DeltaTime) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CQBTacticalIntentTask_Tick);
    ACQBTrainingAIController* Controller = CQBStateTree::ResolveController(Context);
    if (!Controller)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FInstanceDataType& Data = Context.GetInstanceData(*this);
    UCQBTrainingCombatComponent* Combat = Controller->GetCombatComponent();
    if (!CQBStateTree::IsStateSelected(*Controller, ECQBAIBehaviorState::Engage)
        || Combat->GetTacticalIntent() != Data.Intent)
    {
        return EStateTreeRunStatus::Succeeded;
    }

    Combat->TickTacticalIntent(DeltaTime, Data.Intent);
    return Combat->GetTacticalIntent() == Data.Intent
        ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

void FCQBBehaviorStateTask::ExitState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult&) const
{
    const FInstanceDataType& Data = Context.GetInstanceData(*this);
    if (ACQBTrainingAIController* Controller = CQBStateTree::ResolveController(Context))
    {
        CQBStateTree::ExitBehavior(*Controller, Data.State);
    }
}
