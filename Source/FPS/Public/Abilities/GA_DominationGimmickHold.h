#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_DominationGimmickHold.generated.h"

class ADominationPointGimmick;
class APlayerController;
class AShooterPlayerController;

UCLASS()
class FPS_API UGA_DominationGimmickHold : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_DominationGimmickHold();

	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	AShooterPlayerController* ResolvePlayerController(const FGameplayAbilityActorInfo* ActorInfo) const;
	void UnbindInteractionEnded();

	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	void HandleInteractionEnded(APlayerController* PlayerController, bool bCompleted);

	TWeakObjectPtr<APlayerController> OperationController;
	TWeakObjectPtr<ADominationPointGimmick> BoundGimmick;
	FDelegateHandle InteractionEndedDelegateHandle;
	bool bServerInteractionStarted = false;
};
