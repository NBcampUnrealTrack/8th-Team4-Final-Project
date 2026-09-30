#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/RogueInteractable.h"
#include "Training/TrainingTypes.h"
#include "Training/TrainingAreaTransitionTypes.h"
#include "TrainingInteractionActor.generated.h"

class APlayerController;
class ATrainingArena;
class ACQBTrainingPlayerSpawnPoint;
class USoundBase;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UTrainingDialogWidget;

UCLASS()
class FPS_API ATrainingInteractionActor : public AActor, public IRogueInteractable
{
    GENERATED_BODY()

public:
    ATrainingInteractionActor();

    // 복귀 위치를 저장할 때 함께 보관할 출발 구역입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Area Transition")
    FName OriginAreaId = TrainingAreaIds::FreePractice;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool CanInteract(const APlayerController& RequestingPlayer) const override;
    virtual void Interact(APlayerController& RequestingPlayer) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Audio")
    TObjectPtr<USoundBase> InteractionSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
    float InteractionSoundVolume = 1.0f;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Training")
    TObjectPtr<ATrainingArena> Arena;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Training|CQB")
    TObjectPtr<ACQBTrainingPlayerSpawnPoint> CQBEntryPoint;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
    ETrainingInteractionRole InteractionRole = ETrainingInteractionRole::Entrance;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingDialogWidget> DialogWidgetClass;

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
