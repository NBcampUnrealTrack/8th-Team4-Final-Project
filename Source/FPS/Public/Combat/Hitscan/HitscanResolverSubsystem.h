#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"
#include "HitscanResolverSubsystem.generated.h"

class UWorld;
struct FHitscanImpactSample;
struct FRogueHitContext;

UCLASS()
class FPS_API UHitscanResolverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// OutImpactPoints: 이번 발에서 실제로 트레이스가 맞은 지점들(피격 이펙트 스폰용). 대미지 대상이
	// 아닌 벽/환경 충돌도 포함한다 - 대미지 적용 여부와 무관하게 "뭔가에 맞았다"는 사실만 알려준다.
	bool ResolveServerShot(const FHitscanShotRequest& Request, TArray<FVector>& OutImpactPoints) const;

private:
	bool IsValidShotRequest(const UWorld* World, const FHitscanShotRequest& Request) const;
	void ResolveImpactSamples(
		UWorld* World,
		const FHitscanShotRequest& Request,
		TArray<FHitscanImpactSample>& OutImpactSamples) const;
	void ApplyDamageToTargets(
		UWorld* World,
		const FHitscanShotRequest& Request,
		const TArray<FRogueHitContext>& HitContexts) const;
};
