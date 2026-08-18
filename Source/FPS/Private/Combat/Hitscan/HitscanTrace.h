#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/HitscanShotTypes.h"

class UWorld;

struct FHitscanImpactSample
{
	TWeakObjectPtr<AActor> TargetActor;
	FVector ImpactPoint = FVector::ZeroVector;
	FName BoneName = NAME_None;
	FVector ShotDirection = FVector::ForwardVector;
	float WallRetention = 1.0f;
	int32 PelletIndex = 0;
};

// 월드 충돌, 표면 통과, 피해 후보 충돌 추출만 담당한다.
// 대상별 집계와 GAS 호출은 UHitscanResolverSubsystem이 담당한다.
namespace HitscanTrace
{
	bool ResolvePellet(
		UWorld* World,
		const FHitscanShotRequest& Request,
		const FVector& PelletDirection,
		int32 PelletIndex,
		FHitscanImpactSample& OutImpactSample);
}
