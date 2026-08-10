#include "Combat/Hitscan/HitscanPenetrationCalculator.h"

FHitscanPenetrationStepResult FHitscanPenetrationCalculator::ResolveSurface(
	const FHitscanPenetrationState& CurrentState,
	const FHitscanSurfaceSample& SurfaceSample,
	const FHitscanPenetrationSettings& PenetrationSettings)
{
	FHitscanPenetrationStepResult Result;
	Result.ResultingState = CurrentState;

	const float ThicknessCentimeters = SurfaceSample.ThicknessCentimeters;
	const FHitscanSurfaceRule& SurfaceRule = SurfaceSample.SurfaceRule;
	if (!FMath::IsFinite(ThicknessCentimeters)
		|| ThicknessCentimeters <= 0.0f
		|| !FMath::IsFinite(CurrentState.RemainingPenetrationPower)
		|| !FMath::IsFinite(CurrentState.AccumulatedDamageMultiplier)
		|| CurrentState.RemainingPenetrationPower < 0.0f
		|| CurrentState.AccumulatedDamageMultiplier < 0.0f
		|| CurrentState.AccumulatedDamageMultiplier > 1.0f
		|| CurrentState.PenetrationCount < 0
		|| PenetrationSettings.MaximumPenetrationCount <= 0
		|| CurrentState.PenetrationCount >= PenetrationSettings.MaximumPenetrationCount
		|| !FMath::IsFinite(PenetrationSettings.MinimumDamageMultiplierToContinue)
		|| PenetrationSettings.MinimumDamageMultiplierToContinue < 0.0f
		|| PenetrationSettings.MinimumDamageMultiplierToContinue > 1.0f
		|| !FMath::IsFinite(SurfaceRule.Hardness)
		|| SurfaceRule.Hardness < 0.0f
		|| !FMath::IsFinite(SurfaceRule.PenetrationCostPerCentimeter)
		|| SurfaceRule.PenetrationCostPerCentimeter < 0.0f
		|| !FMath::IsFinite(SurfaceRule.DamageRetentionPerCentimeter)
		|| SurfaceRule.DamageRetentionPerCentimeter < 0.0f
		|| SurfaceRule.DamageRetentionPerCentimeter > 1.0f)
	{
		return Result;
	}

	const float Hardness = FMath::Max(0.0f, SurfaceRule.Hardness);
	const float CostPerCentimeter = FMath::Max(0.0f, SurfaceRule.PenetrationCostPerCentimeter);
	const float DamageRetentionPerCentimeter = FMath::Clamp(
		SurfaceRule.DamageRetentionPerCentimeter,
		0.0f,
		1.0f);
	const float PenetrationCost = Hardness + ThicknessCentimeters * CostPerCentimeter;

	Result.PenetrationCost = PenetrationCost;
	if (CurrentState.RemainingPenetrationPower + KINDA_SMALL_NUMBER < PenetrationCost)
	{
		return Result;
	}

	const float SurfaceDamageMultiplier = FMath::Pow(
		DamageRetentionPerCentimeter,
		ThicknessCentimeters);
	const float ResultingDamageMultiplier = CurrentState.AccumulatedDamageMultiplier
		* SurfaceDamageMultiplier;
	const float MinimumDamageMultiplier = FMath::Clamp(
		PenetrationSettings.MinimumDamageMultiplierToContinue,
		0.0f,
		1.0f);

	if (!FMath::IsFinite(SurfaceDamageMultiplier)
		|| !FMath::IsFinite(ResultingDamageMultiplier)
		|| ResultingDamageMultiplier <= KINDA_SMALL_NUMBER
		|| ResultingDamageMultiplier + KINDA_SMALL_NUMBER < MinimumDamageMultiplier)
	{
		return Result;
	}

	Result.bPenetrated = true;
	Result.SurfaceDamageMultiplier = SurfaceDamageMultiplier;
	Result.ResultingState.RemainingPenetrationPower = FMath::Max(
		0.0f,
		CurrentState.RemainingPenetrationPower - PenetrationCost);
	Result.ResultingState.AccumulatedDamageMultiplier = FMath::Max(
		0.0f,
		ResultingDamageMultiplier);
	Result.ResultingState.PenetrationCount = CurrentState.PenetrationCount + 1;
	return Result;
}

FHitscanPenetrationSequenceResult FHitscanPenetrationCalculator::ResolveSequence(
	const float InitialPenetrationPower,
	const TArray<FHitscanSurfaceSample>& SurfaceSamples,
	const FHitscanPenetrationSettings& PenetrationSettings)
{
	FHitscanPenetrationSequenceResult SequenceResult;
	SequenceResult.FinalState.RemainingPenetrationPower = FMath::Max(0.0f, InitialPenetrationPower);
	SequenceResult.FinalState.AccumulatedDamageMultiplier = 1.0f;
	SequenceResult.FinalState.PenetrationCount = 0;

	if (!FMath::IsFinite(InitialPenetrationPower) || InitialPenetrationPower < 0.0f)
	{
		return SequenceResult;
	}

	if (SurfaceSamples.IsEmpty())
	{
		SequenceResult.bReachedEnd = true;
		return SequenceResult;
	}

	for (const FHitscanSurfaceSample& SurfaceSample : SurfaceSamples)
	{
		const FHitscanPenetrationStepResult StepResult = ResolveSurface(
			SequenceResult.FinalState,
			SurfaceSample,
			PenetrationSettings);
		SequenceResult.Steps.Add(StepResult);

		if (!StepResult.bPenetrated)
		{
			return SequenceResult;
		}

		SequenceResult.FinalState = StepResult.ResultingState;
	}

	SequenceResult.bReachedEnd = true;
	return SequenceResult;
}
