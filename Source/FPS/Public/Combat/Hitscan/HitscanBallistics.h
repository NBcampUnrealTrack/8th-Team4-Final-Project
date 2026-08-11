#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/HitscanTypes.h"

class AActor;

struct FHitscanPenetrationState
{
	float RemainingPenetrationPower = 0.0f;
	float AccumulatedDamageMultiplier = 1.0f;
	int32 PenetrationCount = 0;
};

struct FHitscanPenetrationStepResult
{
	bool bPenetrated = false;
	float PenetrationCost = 0.0f;
	float SurfaceDamageMultiplier = 1.0f;
	FHitscanPenetrationState ResultingState;
};

// 상태를 가지지 않는 거리/관통 계산 함수 모음. UObject/월드 Trace와 분리해 단위 테스트 가능하게 둔다.
namespace HitscanBallistics
{
	// BaseDamage에 거리/관통 감쇄만 적용한다. 부위 배율은 GAS ExecCalc가 담당한다.
	float CalculateBallisticDamage(
		float BaseDamage,
		float DistanceMeters,
		float PenetrationDamageMultiplier,
		const FHitscanDistanceFalloffSettings& FalloffSettings);

	FHitscanPenetrationStepResult ResolvePenetration(
		const FHitscanPenetrationState& CurrentState,
		float ThicknessCentimeters,
		const FHitscanSurfaceRule& SurfaceRule,
		const FHitscanPenetrationSettings& PenetrationSettings);

	float CalculateDistanceMultiplier(
		float DistanceMeters,
		const FHitscanDistanceFalloffSettings& FalloffSettings);

	float RoundDamage(
		float Damage,
		const FHitscanDamageRoundingSettings& RoundingSettings);

	// 월드/Actor 없이 방향 벡터만으로 판정하는 순수 함수. 테스트와 실제 판정이 같은 수식을 공유한다.
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
