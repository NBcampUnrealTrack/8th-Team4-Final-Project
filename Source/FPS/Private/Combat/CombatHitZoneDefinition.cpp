#include "Combat/CombatHitZoneDefinition.h"

FGameplayTag UCombatHitZoneDefinition::ResolveRules(
	const TArray<FCombatHitZoneRule>& Rules,
	const FName BoneName,
	const FGameplayTag FallbackTag)
{
	if (BoneName.IsNone())
	{
		return FallbackTag;
	}

	const FCombatHitZoneRule* BestRule = nullptr;
	for (const FCombatHitZoneRule& Rule : Rules)
	{
		if (Rule.HitZoneTag.IsValid()
			&& Rule.BoneNames.Contains(BoneName)
			&& (!BestRule || Rule.Priority > BestRule->Priority))
		{
			BestRule = &Rule;
		}
	}

	if (BestRule)
	{
		return BestRule->HitZoneTag;
	}

	return FallbackTag;
}

FGameplayTag UCombatHitZoneDefinition::ResolveHitZone(
	const FName BoneName,
	const FGameplayTag FallbackTag) const
{
	return ResolveRules(ZoneRules, BoneName, FallbackTag);
}
