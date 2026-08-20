#include "Combat/Hitscan/HitscanHitContextBuilder.h"

#include "Combat/Hitscan/Helpers/HitscanMath.h"
#include "GameFramework/Actor.h"

void FHitscanHitContextBuilder::Build(
	const FHitscanShotRequest& Request,
	const TArray<FHitscanImpactSample>& ImpactSamples,
	TArray<FRogueHitContext>& OutHitContexts)
{
	TArray<FTargetHitAccumulator> TargetAccumulators;
	AccumulateImpactSamples(Request, ImpactSamples, TargetAccumulators);
	FinalizeTargetHitContexts(Request, TargetAccumulators, OutHitContexts);
}

bool FHitscanHitContextBuilder::IsFiniteVector(const FVector& Value)
{
	return FMath::IsFinite(Value.X)
		&& FMath::IsFinite(Value.Y)
		&& FMath::IsFinite(Value.Z);
}

void FHitscanHitContextBuilder::AccumulateImpactSamples(
	const FHitscanShotRequest& Request,
	const TArray<FHitscanImpactSample>& ImpactSamples,
	TArray<FTargetHitAccumulator>& OutAccumulators)
{
	OutAccumulators.Reset();
	TMap<TWeakObjectPtr<AActor>, int32> TargetIndices;

	for (const FHitscanImpactSample& Impact : ImpactSamples)
	{
		AActor* TargetActor = Impact.TargetActor.Get();
		if (!IsValid(TargetActor)
			|| Impact.PelletIndex < 0
			|| Impact.PelletIndex >= Request.PelletDirections.Num()
			|| !IsFiniteVector(Impact.ImpactPoint)
			|| !FMath::IsFinite(Impact.WallRetention))
		{
			continue;
		}

		const float WallRetention = FMath::Clamp(Impact.WallRetention, 0.0f, 1.0f);
		if (WallRetention <= 0.0f)
		{
			continue;
		}

		const TWeakObjectPtr<AActor> TargetKey(TargetActor);
		int32 TargetIndex = INDEX_NONE;
		if (const int32* FoundIndex = TargetIndices.Find(TargetKey))
		{
			TargetIndex = *FoundIndex;
		}
		else
		{
			TargetIndex = OutAccumulators.AddDefaulted();
			OutAccumulators[TargetIndex].HitContext.Target = TargetActor;
			TargetIndices.Add(TargetKey, TargetIndex);
		}

		FTargetHitAccumulator& Accumulator = OutAccumulators[TargetIndex];
		++Accumulator.PelletCount;
		if (Request.DamageSettings.bEnableBackstab
				&& FHitscanMath::IsBackstab(
				TargetActor,
				Impact.ShotDirection,
				Request.DamageSettings.BackstabRearConeHalfAngleDegrees))
		{
			++Accumulator.BackstabPelletCount;
		}

		if (Impact.PelletIndex < Accumulator.FirstPelletIndex)
		{
			Accumulator.FirstPelletIndex = Impact.PelletIndex;
			Accumulator.HitContext.DistanceMeters = FMath::Max(
				0.0f,
				FVector::Distance(Request.DamageDistanceOrigin, Impact.ImpactPoint) / 100.0f);
		}

		const FGameplayTag HitZoneTag = Impact.HitZoneTag;
		if (!HitZoneTag.IsValid())
		{
			continue;
		}

		FRogueHitGroup* HitGroup = Accumulator.HitContext.HitGroups.FindByPredicate(
			[HitZoneTag](const FRogueHitGroup& Group)
			{
				return Group.HitZoneTag.MatchesTagExact(HitZoneTag);
			});

		if (!HitGroup)
		{
			HitGroup = &Accumulator.HitContext.HitGroups.AddDefaulted_GetRef();
			HitGroup->HitZoneTag = HitZoneTag;
			HitGroup->WallRetentionSum = 0.0f;
			HitGroup->PelletCount = 0;
		}

		HitGroup->WallRetentionSum += WallRetention;
		++HitGroup->PelletCount;
	}
}

void FHitscanHitContextBuilder::FinalizeTargetHitContexts(
	const FHitscanShotRequest& Request,
	TArray<FTargetHitAccumulator>& Accumulators,
	TArray<FRogueHitContext>& OutHitContexts)
{
	OutHitContexts.Reset();
	OutHitContexts.Reserve(Accumulators.Num());

	for (FTargetHitAccumulator& Accumulator : Accumulators)
	{
		FRogueHitContext& HitContext = Accumulator.HitContext;
		if (!IsValid(HitContext.Target.Get())
			|| Accumulator.FirstPelletIndex == MAX_int32
			|| !FMath::IsFinite(HitContext.DistanceMeters)
			|| HitContext.DistanceMeters < 0.0f)
		{
			continue;
		}

		HitContext.bBackstab = Request.DamageSettings.bEnableBackstab
			&& Accumulator.PelletCount > 0
			&& Accumulator.BackstabPelletCount * 2 > Accumulator.PelletCount;

		float TotalWallRetention = 0.0f;
		for (int32 GroupIndex = HitContext.HitGroups.Num() - 1; GroupIndex >= 0; --GroupIndex)
		{
			FRogueHitGroup& Group = HitContext.HitGroups[GroupIndex];
			if (!Group.HitZoneTag.IsValid()
				|| Group.PelletCount <= 0
				|| !FMath::IsFinite(Group.WallRetentionSum))
			{
				HitContext.HitGroups.RemoveAtSwap(GroupIndex, 1, EAllowShrinking::No);
				continue;
			}

			Group.WallRetentionSum = FMath::Clamp(
				Group.WallRetentionSum,
				0.0f,
				static_cast<float>(Group.PelletCount));
			TotalWallRetention += Group.WallRetentionSum;
		}

		if (!HitContext.HitGroups.IsEmpty() && TotalWallRetention > 0.0f)
		{
			OutHitContexts.Add(MoveTemp(HitContext));
		}
	}
}
