#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TrainingCombatAIController.generated.h"

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|AI")
    TObjectPtr<UStateTree> StrafeStateTree;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|AI")
    TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

protected:
    virtual void OnUnPossess() override;
};
