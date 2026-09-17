#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "ShooterInteractionComponent.generated.h"

UCLASS(ClassGroup = (Input), meta = (BlueprintSpawnableComponent))
class FPS_API UShooterInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UShooterInteractionComponent();
    bool TryInteract();
    bool CanReachInteractable(const AActor* Candidate) const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "50.0"))
    float InteractionDistanceCentimeters = 300.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Interaction")
    TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_Visibility;

private:
    AActor* FindInteractableInView() const;

    UFUNCTION(Server, Reliable)
    void ServerInteract(AActor* Candidate);
};
