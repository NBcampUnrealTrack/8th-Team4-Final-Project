#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/HitscanTypes.h"

class FPS_API FHitscanPenetrationCalculator final
{
public:
	static FHitscanPenetrationStepResult ResolveSurface(
		const FHitscanPenetrationState& CurrentState,
		const FHitscanSurfaceSample& SurfaceSample,
		const FHitscanPenetrationSettings& PenetrationSettings);

	static FHitscanPenetrationSequenceResult ResolveSequence(
		float InitialPenetrationPower,
		const TArray<FHitscanSurfaceSample>& SurfaceSamples,
		const FHitscanPenetrationSettings& PenetrationSettings);
};
