#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"

class AActor;

struct FHitscanPenetrationState
{
	float AccumulatedWallRetention = 1.0f;
	int32 PenetrationCount = 0;
};

struct FHitscanPenetrationStepResult
{
	bool bPenetrated = false;
	FHitscanPenetrationState ResultingState;
};

class FHitscanMath
{
public:
	static FHitscanPenetrationStepResult ResolvePenetration(
		const FHitscanPenetrationState& CurrentState,
		const FHitscanSurfaceRule& SurfaceRule,
		const FHitscanPenetrationSettings& PenetrationSettings);

	static bool IsBackstabDirection(
		const FVector& TargetFacingDirection,
		const FVector& ShotDirection,
		float RearConeHalfAngleDegrees);

	static bool IsBackstab(
		const AActor* TargetActor,
		const FVector& ShotDirection,
		float RearConeHalfAngleDegrees);

	static bool CalculateRayExitDistanceFromBox(
		const FBox& Box,
		const FVector& Origin,
		const FVector& Direction,
		float& OutExitDistance);

private:
	static FVector ResolveHorizontalFacing(const AActor* TargetActor);
};
