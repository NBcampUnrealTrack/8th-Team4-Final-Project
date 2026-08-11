#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/HitscanShotTypes.h"

class UWorld;

struct FHitscanImpactSample
{
	TWeakObjectPtr<AActor> TargetActor;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::UpVector;
	FName BoneName = NAME_None;
	FVector ShotDirection = FVector::ForwardVector;
	float DistanceMeters = 0.0f;
	float PenetrationDamageMultiplier = 1.0f;
	int32 PelletIndex = 0;
};

// 월드 충돌/표면 통과만 담당한다. 피해 공식은 HitscanBallistics/GAS에 맡긴다.
namespace HitscanTrace
{
	bool ResolvePellet(
		UWorld* World,
		const FHitscanShotRequest& Request,
		const FVector& PelletDirection,
		int32 PelletIndex,
		FHitscanImpactSample& OutImpactSample);
}
