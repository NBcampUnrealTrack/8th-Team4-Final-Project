#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/HitscanTypes.h"

class AActor;

struct FHitscanPenetrationState
{
	float RemainingPenetrationPower = 0.0f;
	float AccumulatedWallRetention = 1.0f;
	int32 PenetrationCount = 0;
};

struct FHitscanPenetrationStepResult
{
	bool bPenetrated = false;
	float PenetrationCost = 0.0f;
	FHitscanPenetrationState ResultingState;
};

// 상태를 가지지 않는 관통/방향 계산 함수 모음. 절대 대미지와 거리 감쇠는 GAS가 담당한다.
namespace HitscanBallistics
{
	FHitscanPenetrationStepResult ResolvePenetration(
		const FHitscanPenetrationState& CurrentState,
		float ThicknessCentimeters,
		const FHitscanSurfaceRule& SurfaceRule,
		const FHitscanPenetrationSettings& PenetrationSettings);

	bool IsBackstabDirection(
		const FVector& TargetFacingDirection,
		const FVector& ShotDirection,
		float RearConeHalfAngleDegrees);

	bool IsBackstab(
		const AActor* TargetActor,
		const FVector& ShotDirection,
		float RearConeHalfAngleDegrees);

	bool CalculateRayExitDistanceFromBox(
		const FBox& Box,
		const FVector& Origin,
		const FVector& Direction,
		float& OutExitDistance);
}

