#pragma once

#include "CoreMinimal.h"
#include "Augment/Abilities/RogueGameplayAbility.h"

#include "RogueSmokeSightAbility.generated.h"

class UMaterialParameterCollection;

UCLASS()
class FPS_API URogueSmokeSightAbility : public URogueGameplayAbility
{
	GENERATED_BODY()

public:
	URogueSmokeSightAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
	                        const FGameplayAbilityActorInfo* ActorInfo,
	                        const FGameplayAbilityActivationInfo ActivationInfo,
	                        bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Outline")
	TObjectPtr<UMaterialParameterCollection> SmokeOcclusionCollection;

private:
	void ApplyOverride(bool bEnable) const;
};
