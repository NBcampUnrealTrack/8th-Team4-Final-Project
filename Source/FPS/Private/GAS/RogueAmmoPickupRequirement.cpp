#include "GAS/RogueAmmoPickupRequirement.h"

#include "AbilitySystemComponent.h"
#include "GAS/RogueAmmoAttributeSet.h"
#include "GameplayEffect.h"
#include "Tags/GameplayTags.h"

bool URogueAmmoPickupRequirement::CanApplyGameplayEffect_Implementation(
	const UGameplayEffect* GameplayEffect, const FGameplayEffectSpec& Spec,
	UAbilitySystemComponent* ASC) const
{
	if (!IsValid(ASC) || !IsValid(GameplayEffect)
		|| GameplayEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		return false;
	}

	const float Magnitude = Spec.GetSetByCallerMagnitude(RogueTags::Data_Magnitude_Primary, false, 0.f);
	int32 Amount = 0;
	return URogueAmmoAttributeSet::ResolveReserveAmmoTarget(*ASC, Spec.GetContext(), Magnitude, Amount) != nullptr;
}
