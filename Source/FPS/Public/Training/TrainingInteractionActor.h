#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/RogueInteractable.h"
#include "Training/TrainingTypes.h"
#include "TrainingInteractionActor.generated.h"

class APlayerController;
class ATrainingArena;
class USoundBase;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS()
class FPS_API ATrainingInteractionActor : public AActor, public IRogueInteractable
{
    GENERATED_BODY()

public:
    ATrainingInteractionActor();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool CanInteract(const APlayerController& RequestingPlayer) const override;
    virtual void Interact(APlayerController& RequestingPlayer) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Audio")
    TObjectPtr<USoundBase> InteractionSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
    float InteractionSoundVolume = 1.0f;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Training")
    TObjectPtr<ATrainingArena> Arena;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
    ETrainingInteractionRole InteractionRole = ETrainingInteractionRole::Entrance;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training")
    TObjectPtr<UBoxComponent> InteractionCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training")
    TObjectPtr<UStaticMeshComponent> DisplayMesh;

protected:
    UFUNCTION()
    void HandleInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void HandleInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    TWeakObjectPtr<APlayerController> LocalOverlappingController;
};
