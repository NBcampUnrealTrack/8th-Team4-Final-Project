#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "TrainingStrafeTask.generated.h"

class AAIController;

USTRUCT()
struct FPS_API FTrainingStrafeTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Context")
    TObjectPtr<AAIController> AIController = nullptr;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bClockwise = false;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.25"))
    float DirectionDurationSeconds = 3.0f;

    UPROPERTY()
    float ElapsedSeconds = 0.0f;

    UPROPERTY()
    float RetrySeconds = 0.0f;
};

USTRUCT(meta = (DisplayName = "Training Radial Strafe", Category = "Training"))
struct FPS_API FTrainingStrafeTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FTrainingStrafeTaskInstanceData;
    FTrainingStrafeTask();
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;
};
