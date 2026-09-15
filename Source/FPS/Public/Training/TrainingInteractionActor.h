#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/RogueInteractable.h"
#include "Training/TrainingTypes.h"
#include "TrainingInteractionActor.generated.h"

class ATrainingArena;
class UBoxComponent;
class UStaticMeshComponent;

// Two placed Blueprint instances: Entrance and ControlPanel. Neither binds input.
UCLASS()
class FPS_API ATrainingInteractionActor : public AActor, public IRogueInteractable
{
    GENERATED_BODY()

public:
    ATrainingInteractionActor();
    virtual bool CanInteract(const APlayerController& RequestingPlayer) const override;
    virtual void Interact(APlayerController& RequestingPlayer) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Training")
    TObjectPtr<ATrainingArena> Arena;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
    ETrainingInteractionRole InteractionRole = ETrainingInteractionRole::Entrance;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training")
    TObjectPtr<UBoxComponent> InteractionCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training")
    TObjectPtr<UStaticMeshComponent> DisplayMesh;
};
