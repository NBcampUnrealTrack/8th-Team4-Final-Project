#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_DataJackHoldAbilityBase.h"
#include "GA_DataJackInteract.generated.h"

UCLASS()
class FPS_API UGA_DataJackInteract : public UGA_DataJackHoldAbilityBase
{
	GENERATED_BODY()

public:
	UGA_DataJackInteract();

	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual EDataJackOperation GetDataJackOperation() const override;
	virtual bool BeginDataJackOperation(
		UBombObjectiveManager& ObjectiveManager,
		APlayerController& PlayerController) override;
	virtual void CancelDataJackOperation(
		UBombObjectiveManager& ObjectiveManager,
		APlayerController& PlayerController) override;
	virtual void HandleDataJackOperationStarted(APlayerController& PlayerController) override;
};
