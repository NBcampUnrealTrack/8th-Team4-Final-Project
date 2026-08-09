#include "Combat/Hitscan/HitZoneDefinition.h"

FGameplayTag UHitscanHitZoneDefinition::ResolveHitZone(const FName BoneName, const FGameplayTag FallbackTag) const
{
	if (BoneName.IsNone())
	{
		return FallbackTag;
	}

	for (const FHitscanHitZoneRule& Rule : HitZoneRules)
	{
		if (Rule.HitZoneTag.IsValid() && Rule.BoneNames.Contains(BoneName))
		{
			return Rule.HitZoneTag;
		}
	}

	return FallbackTag;
}

int32 UHitscanHitZoneDefinition::GetHitZonePriority(const FGameplayTag HitZoneTag) const
{
	for (const FHitscanHitZoneRule& Rule : HitZoneRules)
	{
		if (Rule.HitZoneTag.MatchesTagExact(HitZoneTag))
		{
			return Rule.Priority;
		}
	}

	return 0;
}

bool UHitscanHitZoneDefinition::IsHeadshotZone(const FGameplayTag HitZoneTag) const
{
	for (const FHitscanHitZoneRule& Rule : HitZoneRules)
	{
		if (Rule.HitZoneTag.MatchesTagExact(HitZoneTag))
		{
			return Rule.bCountsAsHeadshot;
		}
	}

	return false;
}
