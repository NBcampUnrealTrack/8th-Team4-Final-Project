#include "Combat/Hitscan/HitZoneDefinition.h"

FHitscanResolvedHitZoneInfo UHitscanHitZoneDefinition::ResolveRules(
	const TArray<FHitscanHitZoneRule>& Rules,
	const FName BoneName,
	const FGameplayTag FallbackTag)
{
	FHitscanResolvedHitZoneInfo Result;
	Result.HitZoneTag = FallbackTag;

	const FHitscanHitZoneRule* BestRule = nullptr;
	if (!BoneName.IsNone())
	{
		for (const FHitscanHitZoneRule& Rule : Rules)
		{
			if (Rule.BoneNames.Contains(BoneName)
				&& (!BestRule || Rule.Priority > BestRule->Priority))
			{
				BestRule = &Rule;
			}
		}
	}

	if (!BestRule && FallbackTag.IsValid())
	{
		for (const FHitscanHitZoneRule& Rule : Rules)
		{
			if (Rule.HitZoneTag.IsValid()
				&& Rule.HitZoneTag.MatchesTagExact(FallbackTag)
				&& (!BestRule || Rule.Priority > BestRule->Priority))
			{
				BestRule = &Rule;
			}
		}
	}

	if (BestRule)
	{
		Result.HitZoneTag = BestRule->HitZoneTag.IsValid()
			? BestRule->HitZoneTag
			: FallbackTag;
		Result.Priority = BestRule->Priority;
		Result.bHeadshot = BestRule->bCountsAsHeadshot;
	}

	return Result;
}

FHitscanResolvedHitZoneInfo UHitscanHitZoneDefinition::ResolveHitZoneInfo(
	const FName BoneName,
	const FGameplayTag FallbackTag) const
{
	return ResolveRules(HitZoneRules, BoneName, FallbackTag);
}

FGameplayTag UHitscanHitZoneDefinition::ResolveHitZone(
	const FName BoneName,
	const FGameplayTag FallbackTag) const
{
	return ResolveHitZoneInfo(BoneName, FallbackTag).HitZoneTag;
}

int32 UHitscanHitZoneDefinition::GetHitZonePriority(const FGameplayTag HitZoneTag) const
{
	if (!HitZoneTag.IsValid())
	{
		return 0;
	}

	int32 HighestPriority = 0;
	for (const FHitscanHitZoneRule& Rule : HitZoneRules)
	{
		if (Rule.HitZoneTag.IsValid() && Rule.HitZoneTag.MatchesTagExact(HitZoneTag))
		{
			HighestPriority = FMath::Max(HighestPriority, Rule.Priority);
		}
	}

	return HighestPriority;
}

bool UHitscanHitZoneDefinition::IsHeadshotZone(const FGameplayTag HitZoneTag) const
{
	if (!HitZoneTag.IsValid())
	{
		return false;
	}

	for (const FHitscanHitZoneRule& Rule : HitZoneRules)
	{
		if (Rule.HitZoneTag.IsValid()
			&& Rule.HitZoneTag.MatchesTagExact(HitZoneTag)
			&& Rule.bCountsAsHeadshot)
		{
			return true;
		}
	}

	return false;
}
