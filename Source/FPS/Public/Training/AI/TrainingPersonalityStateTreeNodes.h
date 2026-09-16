#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "Training/AI/TrainingBotPersonalityTypes.h"
#include "TrainingPersonalityStateTreeNodes.generated.h"

class AAIController;

USTRUCT()
struct FPS_API FTrainingBotConditionInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Context")
    TObjectPtr<AAIController> AIController = nullptr;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    ETrainingBotCondition Test = ETrainingBotCondition::PersonalityIs;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    ETrainingBotPersonality Personality = ETrainingBotPersonality::Copycat;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bExpected = true;
};

USTRUCT(meta = (DisplayName = "Training Personality Condition", Category = "Training"))
struct FPS_API FTrainingBotCondition : public FStateTreeConditionCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FTrainingBotConditionInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct FPS_API FTrainingBotActionInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Context")
    TObjectPtr<AAIController> AIController = nullptr;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    ETrainingBotAction Action = ETrainingBotAction::WaitForParticipant;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "s"))
    float DurationSeconds = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "s"))
    float DurationVariance = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.1", ClampMax = "4.0"))
    float SpeedMultiplier = 1.0f;

    UPROPERTY()
    float ElapsedSeconds = 0.0f;

    UPROPERTY()
    float SelectedDuration = 0.0f;

    UPROPERTY()
    float RetrySeconds = 0.0f;

    UPROPERTY()
    bool bClockwise = false;
};

USTRUCT(meta = (DisplayName = "Training Personality Action", Category = "Training"))
struct FPS_API FTrainingBotActionTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FTrainingBotActionInstanceData;

    FTrainingBotActionTask();

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;
};
