#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Hitscan/HitscanShotTypes.h"
#include "HitscanResolverSubsystem.generated.h"

UCLASS()
class FPS_API UHitscanResolverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// 서버 Trace -> 대상/부위별 WallRetention 집계 -> RogueDamageSubsystem 전달.
	// 절대 대미지, 거리 감쇠, 부위 배율, 방어/Shield 계산은 ExecCalc_Damage가 담당한다.
	FHitscanShotResult ResolveServerShot(const FHitscanShotRequest& Request) const;
};
