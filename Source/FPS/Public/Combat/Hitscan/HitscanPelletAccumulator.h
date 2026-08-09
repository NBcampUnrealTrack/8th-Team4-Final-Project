#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/HitscanTypes.h"

class FPS_API FHitscanPelletAccumulator final
{
public:
	bool AddPellet(const FHitscanResolvedPelletDamage& ResolvedPelletDamage);
	void BuildTargetAggregates(
		const FHitscanDamageRoundingSettings& RoundingSettings,
		TArray<FHitscanTargetDamageAggregate>& OutTargetAggregates) const;
	void Reset();
	int32 NumTargets() const;

private:
	struct FTargetEntry
	{
		TWeakObjectPtr<AActor> TargetActor;
		float TotalDamage = 0.0f;
		float HeadshotDamage = 0.0f;
		float HeadshotBaseDamage = 0.0f;
		float TotalDistanceMeters = 0.0f;
		FGameplayTag RepresentativeHitZoneTag;
		int32 RepresentativeHitZonePriority = MIN_int32;
		bool bIsBackstab = false;
		TSet<int32> PelletIndices;
	};

	TArray<FTargetEntry> TargetEntries;
	TMap<TWeakObjectPtr<AActor>, int32> TargetEntryIndices;
};
