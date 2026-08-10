#include "Combat/Hitscan/HitscanDamageCore.h"

#include "Combat/Hitscan/HitscanDamageCalculator.h"
#include "Combat/Hitscan/HitscanPelletAccumulator.h"

FHitscanDamageCoreResult FHitscanDamageCore::ResolveShot(const FHitscanDamageCoreRequest& Request)
{
	FHitscanDamageCoreResult Result;
	Result.ExpectedPelletCount = FMath::Max(1, Request.ExpectedPelletCount);

	const FHitscanDistanceFalloffSettings& Falloff = Request.DamageSettings.DistanceFalloff;
	const FHitscanDamageRoundingSettings& Rounding = Request.DamageSettings.Rounding;
	if (!FMath::IsFinite(Request.DamageSettings.BaseDamagePerPellet)
		|| Request.DamageSettings.BaseDamagePerPellet <= 0.0f
		|| Request.ImpactSamples.IsEmpty()
		|| !FMath::IsFinite(Falloff.EffectiveRangeMeters)
		|| !FMath::IsFinite(Falloff.MaximumRangeMeters)
		|| !FMath::IsFinite(Falloff.MinimumDamageMultiplierAtMaximumRange)
		|| Falloff.EffectiveRangeMeters < 0.0f
		|| Falloff.MaximumRangeMeters < 0.0f
		|| Falloff.MinimumDamageMultiplierAtMaximumRange < 0.0f
		|| Falloff.MinimumDamageMultiplierAtMaximumRange > 1.0f
		|| (Request.DamageSettings.bEnableBackstab
			&& !FMath::IsFinite(Request.DamageSettings.BackstabRearConeHalfAngleDegrees))
		|| (IsValid(Falloff.DamageMultiplierCurve.Get())
			&& Falloff.EffectiveRangeMeters <= 0.0f)
		|| (Rounding.Mode != EHitscanDamageRoundingMode::None
			&& (!FMath::IsFinite(Rounding.Step) || Rounding.Step <= 0.0f)))
	{
		return Result;
	}

	for (const TPair<FGameplayTag, float>& HitZoneMultiplier : Request.DamageSettings.HitZoneDamageMultipliers)
	{
		if (!FMath::IsFinite(HitZoneMultiplier.Value) || HitZoneMultiplier.Value < 0.0f)
		{
			return Result;
		}
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
