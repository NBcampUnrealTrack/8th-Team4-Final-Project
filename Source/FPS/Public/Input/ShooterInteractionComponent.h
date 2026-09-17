#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "ShooterInteractionComponent.generated.h"

class UUserWidget;

UCLASS(ClassGroup = (Input), meta = (BlueprintSpawnableComponent))
class FPS_API UShooterInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UShooterInteractionComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    bool TryInteract();
    bool CanReachInteractable(const AActor* Candidate) const;
    void RegisterProximityInteractable(AActor* Candidate);
    void UnregisterProximityInteractable(AActor* Candidate);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "50.0"))
    float InteractionDistanceCentimeters = 300.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Interaction")
    TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_Visibility;

    UPROPERTY(EditDefaultsOnly, Category = "Interaction|Prompt")
    TSubclassOf<UUserWidget> InteractionPromptWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Interaction|Prompt", meta = (ClampMin = "0"))
    int32 InteractionPromptZOrder = 25;

    UPROPERTY(EditDefaultsOnly, Category = "Interaction|Prompt")
    TArray<FGameplayTag> InteractionPromptBlockingLayers;

private:
    AActor* FindInteractableInView() const;
    AActor* FindProximityInteractable() const;
    AActor* FindBestInteractable() const;
    bool IsInteractionPromptBlockedByUI() const;
    void RefreshInteractionPrompt();
    void ShowInteractionPrompt();
    void HideInteractionPrompt();

    UFUNCTION(Server, Reliable)
    void ServerInteract(AActor* Candidate);

    TArray<TWeakObjectPtr<AActor>> ProximityInteractables;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ActiveInteractionPrompt;
};
