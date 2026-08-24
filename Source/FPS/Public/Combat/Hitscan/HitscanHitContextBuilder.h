#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"
#include "GAS/Damage/RogueDamageTypes.h"

class FHitscanHitContextBuilder
{
public:
	static void Build(
		const FHitscanShotRequest& Request,
		const TArray<FHitscanImpactSample>& ImpactSamples,
		TArray<FRogueHitContext>& OutHitContexts);

private:
	struct FTargetHitAccumulator
	{
		FRogueHitContext HitContext;
		int32 FirstPelletIndex = MAX_int32;
		int32 PelletCount = 0;
		int32 BackstabPelletCount = 0;
	};

	static bool IsFiniteVector(const FVector& Value);
	static void AccumulateImpactSamples(
		const FHitscanShotRequest& Request,
		const TArray<FHitscanImpactSample>& ImpactSamples,
		TArray<FTargetHitAccumulator>& OutAccumulators);
	static void FinalizeTargetHitContexts(
		const FHitscanShotRequest& Request,
		TArray<FTargetHitAccumulator>& Accumulators,
		TArray<FRogueHitContext>& OutHitContexts);
};
