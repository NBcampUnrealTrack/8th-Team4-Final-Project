#pragma once

#include "CoreMinimal.h"
#include "Training/TrainingCharacter.h"
#include "TrainingTarget.generated.h"

UCLASS()
class FPS_API ATrainingTarget : public ATrainingCharacter
{
    GENERATED_BODY()

public:
    ATrainingTarget();
    virtual void SpawnDefaultController() override;

protected:
    virtual void PostInitializeComponents() override;
    virtual float GetInitialTrainingHealth() const override { return TrainingRules::TargetHealth; }

    virtual bool RemovesOnAnyAcceptedHit() const override { return true; }
};
