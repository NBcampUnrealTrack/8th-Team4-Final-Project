#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "Training/TrainingAreaTransitionTypes.h"
#include "CQBTrainingPlayerSpawnPoint.generated.h"

UCLASS(BlueprintType)
class FPS_API ACQBTrainingPlayerSpawnPoint : public ATargetPoint
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Area Transition")
    FName AreaId = TrainingAreaIds::CQB;
};
