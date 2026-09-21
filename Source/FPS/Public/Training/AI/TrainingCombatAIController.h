#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TrainingCombatAIController.generated.h"

class UTrainingBotPersonalityComponent;
class UStateTree;
class UStateTreeAIComponent;

UCLASS()
class FPS_API ATrainingCombatAIController : public AAIController
{
    GENERATED_BODY()

public:
    ATrainingCombatAIController();
    bool HasConfiguredStateTree() const;
    bool StartTrainingBrain();
    void StopTrainingBrain();
    void RefreshParticipantFocus();
    bool RequestStrafeStep(bool bClockwise);
    bool ValidateCurrentMove();
    bool RequestPersonalityStep(bool bClockwise, float RadialStepCentimeters);
    bool ShouldExtendPersonalityMove() const;
    void SetTrainingSpeedMultiplier(float Multiplier);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|Personality")
    TObjectPtr<UTrainingBotPersonalityComponent> PersonalityComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|AI")
    TObjectPtr<UStateTree> StrafeStateTree;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|AI")
    TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

protected:
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool RequestTrainingDestination(const FVector& Destination);
    void RestoreTrainingSpeed();
    float OriginalTrainingSpeed = 0.0f;
    bool bHasOriginalTrainingSpeed = false;
};
