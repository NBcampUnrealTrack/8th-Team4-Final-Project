#include "Combat/Hitscan/HitscanPelletAccumulator.h"

#include "Combat/Hitscan/HitscanDamageCalculator.h"
#include "GameFramework/Actor.h"

bool FHitscanPelletAccumulator::AddPellet(const FHitscanResolvedPelletDamage& ResolvedPelletDamage)
{
	AActor* TargetActor = ResolvedPelletDamage.TargetActor.Get();
	if (!IsValid(TargetActor)
		|| ResolvedPelletDamage.PelletIndex < 0
		|| ResolvedPelletDamage.FinalDamage <= 0.0f
		|| !FMath::IsFinite(ResolvedPelletDamage.FinalDamage)
		|| !FMath::IsFinite(ResolvedPelletDamage.DistanceMeters))
	{
		return false;
	}

	const TWeakObjectPtr<AActor> TargetKey(TargetActor);
	int32 TargetEntryIndex = INDEX_NONE;
	if (const int32* FoundTargetEntryIndex = TargetEntryIndices.Find(TargetKey))
	{
		TargetEntryIndex = *FoundTargetEntryIndex;
	}
	else
	{
		TargetEntryIndex = TargetEntries.AddDefaulted();
		TargetEntries[TargetEntryIndex].TargetActor = TargetActor;
		TargetEntryIndices.Add(TargetKey, TargetEntryIndex);
	}

	FTargetEntry& TargetEntry = TargetEntries[TargetEntryIndex];
	if (TargetEntry.PelletIndices.Contains(ResolvedPelletDamage.PelletIndex))
	{
		return false;
	}

	TargetEntry.PelletIndices.Add(ResolvedPelletDamage.PelletIndex);
	TargetEntry.TotalDamage += ResolvedPelletDamage.FinalDamage;
	TargetEntry.TotalDistanceMeters += FMath::Max(0.0f, ResolvedPelletDamage.DistanceMeters);
	TargetEntry.bIsBackstab |= ResolvedPelletDamage.bBackstab;

	if (ResolvedPelletDamage.bHeadshot)
	{
		TargetEntry.HeadshotDamage += ResolvedPelletDamage.FinalDamage;
		TargetEntry.HeadshotBaseDamage += ResolvedPelletDamage.DamageBeforeHitZoneMultiplier;
	}

	if (!TargetEntry.RepresentativeHitZoneTag.IsValid()
		|| ResolvedPelletDamage.HitZonePriority > TargetEntry.RepresentativeHitZonePriority)
	{
		TargetEntry.RepresentativeHitZoneTag = ResolvedPelletDamage.HitZoneTag;
		TargetEntry.RepresentativeHitZonePriority = ResolvedPelletDamage.HitZonePriority;
	}

	return true;
}

void FHitscanPelletAccumulator::BuildTargetAggregates(
	const FHitscanDamageRoundingSettings& RoundingSettings,
	TArray<FHitscanTargetDamageAggregate>& OutTargetAggregates) const
{
	OutTargetAggregates.Reset(TargetEntries.Num());

	for (const FTargetEntry& TargetEntry : TargetEntries)
	{
		AActor* TargetActor = TargetEntry.TargetActor.Get();
		const int32 PelletsHit = TargetEntry.PelletIndices.Num();
		if (!IsValid(TargetActor) || PelletsHit <= 0)
		{
			continue;
		}

		const float RoundedBaseDamage = FHitscanDamageCalculator::RoundDamage(
			TargetEntry.TotalDamage,
			RoundingSettings);
		const float RoundedHeadshotDamage = FMath::Min(
			RoundedBaseDamage,
			FHitscanDamageCalculator::RoundDamage(TargetEntry.HeadshotDamage, RoundingSettings));
		const float RoundedHeadshotBaseDamage = FHitscanDamageCalculator::RoundDamage(
			TargetEntry.HeadshotBaseDamage,
			RoundingSettings);
		if (RoundedBaseDamage <= 0.0f)
		{
			continue;
		}

		FHitscanTargetDamageAggregate& TargetAggregate = OutTargetAggregates.AddDefaulted_GetRef();
		TargetAggregate.TargetActor = TargetActor;
		TargetAggregate.BaseDamage = RoundedBaseDamage;
		TargetAggregate.HeadshotDamage = RoundedHeadshotDamage;
		TargetAggregate.AppliedHeadshotMultiplier = RoundedHeadshotBaseDamage > KINDA_SMALL_NUMBER
			? FMath::Max(KINDA_SMALL_NUMBER, RoundedHeadshotDamage / RoundedHeadshotBaseDamage)
			: 1.0f;
		TargetAggregate.RepresentativeHitZoneTag = TargetEntry.RepresentativeHitZoneTag;
		TargetAggregate.DistanceMeters = TargetEntry.TotalDistanceMeters / static_cast<float>(PelletsHit);
		TargetAggregate.PelletsHit = PelletsHit;
		TargetAggregate.bIsBackstab = TargetEntry.bIsBackstab;
	}
}

void FHitscanPelletAccumulator::Reset()
{
	TargetEntries.Reset();
	TargetEntryIndices.Reset();
}

int32 FHitscanPelletAccumulator::NumTargets() const
{
	return TargetEntries.Num();
}
