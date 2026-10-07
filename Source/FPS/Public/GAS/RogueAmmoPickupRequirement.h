#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectCustomApplicationRequirement.h"
#include "RogueAmmoPickupRequirement.generated.h"

// 지급할 대상이 없으면 GE 적용을 거절해 공용 픽업이 소비되지 않게 한다.
UCLASS()
class FPS_API URogueAmmoPickupRequirement : public UGameplayEffectCustomApplicationRequirement
{
	GENERATED_BODY()

public:
	virtual bool CanApplyGameplayEffect_Implementation(
		const UGameplayEffect* GameplayEffect, const FGameplayEffectSpec& Spec,
		UAbilitySystemComponent* ASC) const override;
};
