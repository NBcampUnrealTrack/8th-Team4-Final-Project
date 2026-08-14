#include "Combat/Hitscan/HitscanBallistics.h"

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
		|| !FMath::IsFinite(CurrentState.AccumulatedWallRetention)
		|| CurrentState.RemainingPenetrationPower < 0.0f
		|| CurrentState.AccumulatedWallRetention < 0.0f
		|| CurrentState.AccumulatedWallRetention > 1.0f
		|| CurrentState.PenetrationCount < 0
		|| PenetrationSettings.MaximumPenetrationCount <= 0
		|| CurrentState.PenetrationCount >= PenetrationSettings.MaximumPenetrationCount
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

	const float SurfaceWallRetention = FMath::Pow(
		FMath::Clamp(SurfaceRule.DamageRetentionPerCentimeter, 0.0f, 1.0f),
		ThicknessCentimeters);
	const float ResultingWallRetention = CurrentState.AccumulatedWallRetention
		* SurfaceWallRetention;

	// 로컬 임계값으로 작은 양수 계수를 버리지 않는다. 실제 보존 계수는 그대로 GAS에 전달한다.
	if (!FMath::IsFinite(SurfaceWallRetention)
		|| !FMath::IsFinite(ResultingWallRetention)
		|| ResultingWallRetention <= 0.0f)
	{
		return Result;
	}

	Result.bPenetrated = true;
	Result.ResultingState.RemainingPenetrationPower = FMath::Max(
		0.0f,
		CurrentState.RemainingPenetrationPower - PenetrationCost);
	Result.ResultingState.AccumulatedWallRetention = FMath::Clamp(
		ResultingWallRetention,
		0.0f,
		1.0f);
	Result.ResultingState.PenetrationCount = CurrentState.PenetrationCount + 1;
	return Result;
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
