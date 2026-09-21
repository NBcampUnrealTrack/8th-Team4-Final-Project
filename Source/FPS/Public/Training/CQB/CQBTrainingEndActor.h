#pragma once

#include "CoreMinimal.h"
#include "Training/UI/CQBTrainingDialogWidget.h"
#include "GameFramework/Actor.h"
#include "Interfaces/RogueInteractable.h"
#include "CQBTrainingEndActor.generated.h"

class ACQBTrainingSpawner;
class APlayerController;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS()
class FPS_API ACQBTrainingEndActor : public AActor, public IRogueInteractable
{
    GENERATED_BODY()

public:
    ACQBTrainingEndActor();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool CanInteract(const APlayerController& RequestingPlayer) const override;
    virtual void Interact(APlayerController& RequestingPlayer) override;

    void SetEndEnabled(bool bEnabled);
    bool IsEndEnabled() const { return bEndEnabled; }

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CQB")
    TObjectPtr<ACQBTrainingSpawner> Spawner;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CQB|UI")
    TSubclassOf<UCQBTrainingDialogWidget> EndDialogWidgetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|Interaction")
    TObjectPtr<UBoxComponent> InteractionCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|Interaction")
    TObjectPtr<UStaticMeshComponent> DisplayMesh;

protected:
    UFUNCTION()
    void HandleInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void HandleInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    void RefreshEnabledState();

    UPROPERTY(Transient)
    bool bEndEnabled = false;

    TWeakObjectPtr<APlayerController> LocalOverlappingController;
};
