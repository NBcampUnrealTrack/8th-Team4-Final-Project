#include "Combat/Hitscan/Helpers/HitscanMath.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

FVector FHitscanMath::ResolveHorizontalFacing(const AActor* TargetActor)
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

FHitscanPenetrationStepResult FHitscanMath::ResolvePenetration(
	const FHitscanPenetrationState& CurrentState,
	const FHitscanSurfaceRule& SurfaceRule,
	const FHitscanPenetrationSettings& PenetrationSettings)
{
	FHitscanPenetrationStepResult Result;
	Result.ResultingState = CurrentState;

	if (!FMath::IsFinite(CurrentState.AccumulatedWallRetention)
		|| CurrentState.AccumulatedWallRetention < 0.0f
		|| CurrentState.AccumulatedWallRetention > 1.0f
		|| CurrentState.PenetrationCount < 0
		|| PenetrationSettings.MaximumPenetrationCount <= 0
		|| CurrentState.PenetrationCount >= PenetrationSettings.MaximumPenetrationCount
		|| !SurfaceRule.bCanPenetrate
		|| !FMath::IsFinite(SurfaceRule.DamageMultiplier)
		|| SurfaceRule.DamageMultiplier <= 0.0f
		|| SurfaceRule.DamageMultiplier > 1.0f)
	{
		return Result;
	}

	const float ResultingWallRetention = CurrentState.AccumulatedWallRetention
		* FMath::Clamp(SurfaceRule.DamageMultiplier, 0.0f, 1.0f);
	if (!FMath::IsFinite(ResultingWallRetention) || ResultingWallRetention <= 0.0f)
	{
		return Result;
	}

	Result.bPenetrated = true;
	Result.ResultingState.AccumulatedWallRetention = FMath::Clamp(
		ResultingWallRetention,
		0.0f,
		1.0f);
	Result.ResultingState.PenetrationCount = CurrentState.PenetrationCount + 1;
	return Result;
}

bool FHitscanMath::IsBackstabDirection(
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

bool FHitscanMath::IsBackstab(
	const AActor* TargetActor,
	const FVector& ShotDirection,
	const float RearConeHalfAngleDegrees)
{
	return IsBackstabDirection(
		ResolveHorizontalFacing(TargetActor),
		ShotDirection,
		RearConeHalfAngleDegrees);
}

bool FHitscanMath::CalculateRayExitDistanceFromBox(
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
