#include "Combat/Hitscan/HitZoneDefinition.h"

FGameplayTag UHitscanHitZoneDefinition::ResolveRules(
	const TArray<FHitscanHitZoneRule>& Rules,
	const FName BoneName,
	const FGameplayTag FallbackTag)
{
	if (BoneName.IsNone())
	{
		return FallbackTag;
	}

	const FHitscanHitZoneRule* BestRule = nullptr;
	for (const FHitscanHitZoneRule& Rule : Rules)
	{
		if (Rule.HitZoneTag.IsValid()
			&& Rule.BoneNames.Contains(BoneName)
			&& (!BestRule || Rule.Priority > BestRule->Priority))
		{
			BestRule = &Rule;
		}
	}

	return BestRule ? BestRule->HitZoneTag : FallbackTag;
}

FGameplayTag UHitscanHitZoneDefinition::ResolveHitZone(
	const FName BoneName,
	const FGameplayTag FallbackTag) const
{
	return ResolveRules(HitZoneRules, BoneName, FallbackTag);
}
