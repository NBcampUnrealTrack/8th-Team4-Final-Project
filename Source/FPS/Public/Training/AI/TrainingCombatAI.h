#pragma once

#include "CoreMinimal.h"
#include "Training/TrainingCharacter.h"
#include "TrainingCombatAI.generated.h"

class APlayerController;
class ATrainingArena;

UCLASS()
class FPS_API ATrainingCombatAI : public ATrainingCharacter
{
    GENERATED_BODY()

public:
    ATrainingCombatAI();
    bool InitializeForTraining(ATrainingArena& InArena, APlayerController& InParticipant,
        const FTrainingDistanceBand& InBand);
    bool HasConfiguredBehavior() const;
    ATrainingArena* GetTrainingArena() const;
    APlayerController* GetParticipant() const;
    const FTrainingDistanceBand& GetDistanceBand() const { return DistanceBand; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void StopTrainingMotion() override;

private:
    TWeakObjectPtr<ATrainingArena> TrainingArena;
    TWeakObjectPtr<APlayerController> Participant;
    FTrainingDistanceBand DistanceBand;
};
