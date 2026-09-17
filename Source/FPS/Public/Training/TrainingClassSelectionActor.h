#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/RogueInteractable.h"
#include "TrainingClassSelectionActor.generated.h"

class APlayerController;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS()
class FPS_API ATrainingClassSelectionActor : public AActor, public IRogueInteractable
{
    GENERATED_BODY()

public:
    ATrainingClassSelectionActor();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool CanInteract(const APlayerController& RequestingPlayer) const override;
    virtual void Interact(APlayerController& RequestingPlayer) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|Class Selection")
    TObjectPtr<UBoxComponent> InteractionCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|Class Selection")
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
