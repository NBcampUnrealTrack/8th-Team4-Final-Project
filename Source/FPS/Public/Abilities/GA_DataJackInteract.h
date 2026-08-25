#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_DataJackHoldAbilityBase.h"
#include "GameplayTagContainer.h"
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
	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual EDataJackOperation GetDataJackOperation() const override;
	virtual bool BeginDataJackOperation(
		UBombObjectiveManager& ObjectiveManager,
		APlayerController& PlayerController) override;
	virtual void CancelDataJackOperation(
		UBombObjectiveManager& ObjectiveManager,
		APlayerController& PlayerController) override;
	virtual void HandleDataJackOperationStarted(APlayerController& PlayerController) override;

private:
	// 이전 태그를 걷어내고 새 태그를 붙인다. 무효 태그를 넣으면 제거만 한다.
	void SetInteractionStateTag(const FGameplayAbilityActorInfo* ActorInfo, FGameplayTag NewStateTag);

	FGameplayTag AppliedInteractionStateTag;
};
