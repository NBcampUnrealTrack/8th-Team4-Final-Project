#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "Training/CQB/CQBTrainingAIController.h"
#include "CQBTrainingStateTreeNodes.generated.h"

USTRUCT()
struct FPS_API FCQBBehaviorConditionInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "CQB")
    ECQBAIBehaviorState DesiredState = ECQBAIBehaviorState::Patrol;

    UPROPERTY(EditAnywhere, Category = "CQB")
    bool bExpected = true;
};

USTRUCT(meta = (DisplayName = "CQB Behavior State Condition", Category = "CQB"))
struct FPS_API FCQBBehaviorStateCondition : public FStateTreeConditionCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FCQBBehaviorConditionInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct FPS_API FCQBBehaviorTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "CQB")
    ECQBAIBehaviorState State = ECQBAIBehaviorState::Patrol;
};

USTRUCT(meta = (DisplayName = "CQB Behavior State", Category = "CQB"))
struct FPS_API FCQBBehaviorStateTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FCQBBehaviorTaskInstanceData;

    FCQBBehaviorStateTask();

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;
};
