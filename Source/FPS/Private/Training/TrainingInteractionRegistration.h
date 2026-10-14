#pragma once

#include "CoreMinimal.h"

class AActor;
class APlayerController;
class UBoxComponent;
class UStaticMeshComponent;

namespace TrainingInteractionRegistration
{
    void ConfigureCollision(UBoxComponent& Collision, const FVector& DefaultExtent);
    void ConfigureDisplayMesh(UStaticMeshComponent& DisplayMesh, UBoxComponent& Parent);
    void Register(AActor& Interactable, AActor* OtherActor,
        TWeakObjectPtr<APlayerController>& InOutLocalController);
    void Unregister(AActor& Interactable, AActor* OtherActor,
        TWeakObjectPtr<APlayerController>& InOutLocalController);
    void Cleanup(AActor& Interactable, TWeakObjectPtr<APlayerController>& InOutLocalController);
}
