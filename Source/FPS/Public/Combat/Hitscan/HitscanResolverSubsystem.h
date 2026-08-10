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

	// 서버 Trace -> 거리/관통 계산 -> HitZone 메타데이터 집계 -> RogueDamageSubsystem 전달.
	// Resolver는 부위 배율을 곱하지 않는다. 실제 부위 배율/증강/방어/Shield 계산은 ExecCalc_Damage가 담당한다.
	FHitscanShotResult ResolveServerShot(const FHitscanShotRequest& Request) const;
};
