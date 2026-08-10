#include "Combat/Hitscan/HitscanBallistics.h"

#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

namespace
{
	FVector ResolveHorizontalFacing(const AActor* TargetActor)
	{
		if (!IsValid(TargetActor))
		{
			return FVector::ZeroVector;
		}

		FVector Facing = TargetActor->GetActorForwardVector();
		if (const APawn* Pawn = Cast<APawn>(TargetActor))
		{
			Facing = Pawn->GetBaseAimRotation().Vector();
		}

		Facing.Z = 0.0f;
		return Facing.GetSafeNormal();
	}
}

float HitscanBallistics::CalculateBallisticDamage(
	const float BaseDamage,
	const float DistanceMeters,
	const float PenetrationDamageMultiplier,
	const FHitscanDistanceFalloffSettings& FalloffSettings)
{
	if (!FMath::IsFinite(BaseDamage)
		|| BaseDamage <= 0.0f
		|| !FMath::IsFinite(DistanceMeters)
		|| DistanceMeters < 0.0f
		|| !FMath::IsFinite(PenetrationDamageMultiplier)
		|| PenetrationDamageMultiplier < 0.0f
		|| PenetrationDamageMultiplier > 1.0f)
	{
		return 0.0f;
	}

	const float DistanceMultiplier = CalculateDistanceMultiplier(DistanceMeters, FalloffSettings);
	const float Damage = BaseDamage
		* DistanceMultiplier
		* FMath::Clamp(PenetrationDamageMultiplier, 0.0f, 1.0f);

	return FMath::IsFinite(Damage) ? FMath::Max(0.0f, Damage) : 0.0f;
}

FHitscanPenetrationStepResult HitscanBallistics::ResolvePenetration(
	const FHitscanPenetrationState& CurrentState,
	const float ThicknessCentimeters,
	const FHitscanSurfaceRule& SurfaceRule,
	const FHitscanPenetrationSettings& PenetrationSettings)
{
	FHitscanPenetrationStepResult Result;
	Result.ResultingState = CurrentState;

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

	const float PenetrationCost = SurfaceRule.Hardness
		+ ThicknessCentimeters * SurfaceRule.PenetrationCostPerCentimeter;
	Result.PenetrationCost = PenetrationCost;

	if (CurrentState.RemainingPenetrationPower + KINDA_SMALL_NUMBER < PenetrationCost)
	{
		return Result;
	}

	const float SurfaceDamageMultiplier = FMath::Pow(
		FMath::Clamp(SurfaceRule.DamageRetentionPerCentimeter, 0.0f, 1.0f),
		ThicknessCentimeters);
	const float ResultingDamageMultiplier = CurrentState.AccumulatedDamageMultiplier
		* SurfaceDamageMultiplier;

	if (!FMath::IsFinite(SurfaceDamageMultiplier)
		|| !FMath::IsFinite(ResultingDamageMultiplier)
		|| ResultingDamageMultiplier <= KINDA_SMALL_NUMBER
		|| ResultingDamageMultiplier + KINDA_SMALL_NUMBER
			< PenetrationSettings.MinimumDamageMultiplierToContinue)
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

float HitscanBallistics::CalculateDistanceMultiplier(
	const float DistanceMeters,
	const FHitscanDistanceFalloffSettings& FalloffSettings)
{
	const float SafeDistanceMeters = FMath::Max(0.0f, DistanceMeters);
	const float EffectiveRangeMeters = FMath::Max(0.0f, FalloffSettings.EffectiveRangeMeters);
	const float MaximumRangeMeters = FMath::Max(EffectiveRangeMeters, FalloffSettings.MaximumRangeMeters);
	const float MinimumMultiplier = FMath::Clamp(
		FalloffSettings.MinimumDamageMultiplierAtMaximumRange,
		0.0f,
		1.0f);

	if (IsValid(FalloffSettings.DamageMultiplierCurve.Get()))
	{
		if (EffectiveRangeMeters <= KINDA_SMALL_NUMBER)
		{
			return 1.0f;
		}

		float CurveInput = SafeDistanceMeters / EffectiveRangeMeters;
		if (MaximumRangeMeters > EffectiveRangeMeters + KINDA_SMALL_NUMBER)
		{
			CurveInput = FMath::Min(CurveInput, MaximumRangeMeters / EffectiveRangeMeters);
		}

		const float CurveMultiplier = FalloffSettings.DamageMultiplierCurve->GetFloatValue(CurveInput);
		return FMath::Clamp(
			FMath::IsFinite(CurveMultiplier) ? CurveMultiplier : 1.0f,
			MinimumMultiplier,
			1.0f);
	}

	if (MaximumRangeMeters <= KINDA_SMALL_NUMBER || SafeDistanceMeters <= EffectiveRangeMeters)
	{
		return 1.0f;
	}

	if (MaximumRangeMeters <= EffectiveRangeMeters + KINDA_SMALL_NUMBER)
	{
		return MinimumMultiplier;
	}

	const float FalloffAlpha = FMath::Clamp(
		(SafeDistanceMeters - EffectiveRangeMeters)
		/ (MaximumRangeMeters - EffectiveRangeMeters),
		0.0f,
		1.0f);
	return FMath::Lerp(1.0f, MinimumMultiplier, FalloffAlpha);
}

float HitscanBallistics::RoundDamage(
	const float Damage,
	const FHitscanDamageRoundingSettings& RoundingSettings)
{
	const float SafeDamage = FMath::Max(0.0f, Damage);
	if (RoundingSettings.Mode == EHitscanDamageRoundingMode::None)
	{
		return SafeDamage;
	}

	const float Step = FMath::Max(RoundingSettings.Step, KINDA_SMALL_NUMBER);
	const float ScaledDamage = SafeDamage / Step;

	switch (RoundingSettings.Mode)
	{
	case EHitscanDamageRoundingMode::Nearest:
		return FMath::RoundToFloat(ScaledDamage) * Step;
	case EHitscanDamageRoundingMode::Floor:
		return FMath::FloorToFloat(ScaledDamage) * Step;
	case EHitscanDamageRoundingMode::Ceil:
		return FMath::CeilToFloat(ScaledDamage) * Step;
	case EHitscanDamageRoundingMode::None:
	default:
		return SafeDamage;
	}
}

bool HitscanBallistics::IsBackstabDirection(
	const FVector& TargetFacingDirection,
	const FVector& ShotDirection,
	const float RearConeHalfAngleDegrees)
{
	FVector HorizontalFacing = TargetFacingDirection;
	HorizontalFacing.Z = 0.0f;
	HorizontalFacing = HorizontalFacing.GetSafeNormal();

	FVector HorizontalShotDirection = ShotDirection;
	HorizontalShotDirection.Z = 0.0f;
	HorizontalShotDirection = HorizontalShotDirection.GetSafeNormal();

	if (HorizontalFacing.IsNearlyZero() || HorizontalShotDirection.IsNearlyZero())
	{
		return false;
	}

	const float ClampedHalfAngle = FMath::Clamp(RearConeHalfAngleDegrees, 0.0f, 180.0f);
	const float MinimumAlignment = FMath::Cos(FMath::DegreesToRadians(ClampedHalfAngle));
	return FVector::DotProduct(HorizontalFacing, HorizontalShotDirection) >= MinimumAlignment;
}

bool HitscanBallistics::IsBackstab(
	const AActor* TargetActor,
	const FVector& ShotDirection,
	const float RearConeHalfAngleDegrees)
{
	return IsBackstabDirection(
		ResolveHorizontalFacing(TargetActor),
		ShotDirection,
		RearConeHalfAngleDegrees);
}

bool HitscanBallistics::CalculateRayExitDistanceFromBox(
	const FBox& Box,
	const FVector& Origin,
	const FVector& Direction,
	float& OutExitDistance)
{
	OutExitDistance = 0.0f;
	if (!Box.IsValid || Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector RayDirection = Direction.GetSafeNormal();
	float TMin = 0.0f;
	float TMax = TNumericLimits<float>::Max();

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float OriginAxis = Origin[Axis];
		const float DirectionAxis = RayDirection[Axis];
		const float MinAxis = Box.Min[Axis];
		const float MaxAxis = Box.Max[Axis];

		if (FMath::Abs(DirectionAxis) <= KINDA_SMALL_NUMBER)
		{
			if (OriginAxis < MinAxis || OriginAxis > MaxAxis)
			{
				return false;
			}
			continue;
		}

		float T1 = (MinAxis - OriginAxis) / DirectionAxis;
		float T2 = (MaxAxis - OriginAxis) / DirectionAxis;
		if (T1 > T2)
		{
			Swap(T1, T2);
		}

		TMin = FMath::Max(TMin, T1);
		TMax = FMath::Min(TMax, T2);
		if (TMin > TMax)
		{
			return false;
		}
	}

	if (!FMath::IsFinite(TMax) || TMax <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutExitDistance = TMax;
	return true;
}
