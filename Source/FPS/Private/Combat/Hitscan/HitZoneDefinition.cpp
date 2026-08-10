#include "Combat/Hitscan/HitZoneDefinition.h"

FHitscanResolvedHitZoneInfo FHitscanHitZoneResolver::Resolve(
	const TArray<FHitscanHitZoneRule>& Rules,
	const FName BoneName,
	const FGameplayTag FallbackTag)
{
	FHitscanResolvedHitZoneInfo Result;
	Result.HitZoneTag = FallbackTag;

	if (!BoneName.IsNone())
	{
		for (const FHitscanHitZoneRule& Rule : Rules)
		{
			if (Rule.BoneNames.Contains(BoneName))
			{
				// GameplayTag가 아직 등록되지 않았어도 Bone 규칙의 Priority/Headshot 정보는 유지한다.
				// 매칭된 Rule의 Tag가 비어 있으면 잘못된 Fallback Tag로 위장하지 않고 비어 있는 상태를 유지한다.
				Result.HitZoneTag = Rule.HitZoneTag;
				Result.Priority = Rule.Priority;
				Result.bHeadshot = Rule.bCountsAsHeadshot;
				return Result;
			}
		}
	}

	if (FallbackTag.IsValid())
	{
		for (const FHitscanHitZoneRule& Rule : Rules)
		{
			if (Rule.HitZoneTag.IsValid() && Rule.HitZoneTag.MatchesTagExact(FallbackTag))
			{
				Result.Priority = Rule.Priority;
				Result.bHeadshot = Rule.bCountsAsHeadshot;
				break;
			}
		}
	}

	return Result;
}

FHitscanResolvedHitZoneInfo UHitscanHitZoneDefinition::ResolveHitZoneInfo(
	const FName BoneName,
	const FGameplayTag FallbackTag) const
{
	return FHitscanHitZoneResolver::Resolve(HitZoneRules, BoneName, FallbackTag);
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

	for (const FHitscanHitZoneRule& Rule : HitZoneRules)
	{
		if (Rule.HitZoneTag.IsValid() && Rule.HitZoneTag.MatchesTagExact(HitZoneTag))
		{
			return Rule.Priority;
		}
	}

	return 0;
}

bool UHitscanHitZoneDefinition::IsHeadshotZone(const FGameplayTag HitZoneTag) const
{
	if (!HitZoneTag.IsValid())
	{
		return false;
	}

	for (const FHitscanHitZoneRule& Rule : HitZoneRules)
	{
		if (Rule.HitZoneTag.IsValid() && Rule.HitZoneTag.MatchesTagExact(HitZoneTag))
		{
			return Rule.bCountsAsHeadshot;
		}
	}

	return false;
}
