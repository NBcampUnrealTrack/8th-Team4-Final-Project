#include "Combat/Hitscan/HitscanDamageCore.h"

#include "Combat/Hitscan/HitscanDamageCalculator.h"
#include "Combat/Hitscan/HitscanPelletAccumulator.h"

FHitscanDamageCoreResult FHitscanDamageCore::ResolveShot(const FHitscanDamageCoreRequest& Request)
{
	FHitscanDamageCoreResult Result;
	Result.ExpectedPelletCount = FMath::Max(1, Request.ExpectedPelletCount);

	if (Request.DamageSettings.BaseDamagePerPellet <= 0.0f
		|| Request.ImpactSamples.IsEmpty()
		|| (IsValid(Request.DamageSettings.DistanceFalloff.DamageMultiplierCurve.Get())
			&& Request.DamageSettings.DistanceFalloff.EffectiveRangeMeters <= 0.0f)
		|| (Request.DamageSettings.Rounding.Mode != EHitscanDamageRoundingMode::None
			&& Request.DamageSettings.Rounding.Step <= 0.0f))
	{
		return Result;
	}

	FHitscanPelletAccumulator PelletAccumulator;
	Result.ResolvedPellets.Reserve(Request.ImpactSamples.Num());

	for (const FHitscanImpactSample& ImpactSample : Request.ImpactSamples)
	{
		if (ImpactSample.PelletIndex < 0 || ImpactSample.PelletIndex >= Result.ExpectedPelletCount)
		{
			continue;
		}

		FHitscanResolvedPelletDamage ResolvedPelletDamage;
		if (!FHitscanDamageCalculator::ResolvePelletDamage(
			ImpactSample,
			Request.DamageSettings,
			ResolvedPelletDamage))
		{
			continue;
		}

		if (PelletAccumulator.AddPellet(ResolvedPelletDamage))
		{
			Result.ResolvedPellets.Add(ResolvedPelletDamage);
		}
	}

	PelletAccumulator.BuildTargetAggregates(
		Request.DamageSettings.Rounding,
		Result.TargetAggregates);
	Result.ResolvedImpactCount = Result.ResolvedPellets.Num();
	Result.bProcessed = true;
	return Result;
}
