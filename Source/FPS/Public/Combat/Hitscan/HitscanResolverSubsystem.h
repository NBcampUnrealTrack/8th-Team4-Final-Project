#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Hitscan/HitscanShotTypes.h"
#include "HitscanResolverSubsystem.generated.h"

struct FHitResult;
class UPrimitiveComponent;

UCLASS()
class FPS_API UHitscanResolverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 서버에서 승인된 한 번의 사격을 처리한다.
	// TODO(A 담당): AWeaponBase에서 펠릿별 최종 방향을 만든 뒤 이 함수를 한 번만 호출한다.
	FHitscanShotResult ResolveServerShot(const FHitscanShotRequest& Request) const;

private:
	bool IsRequestValid(const FHitscanShotRequest& Request) const;

	FHitscanPelletTraceResult ResolvePellet(
		const FHitscanShotRequest& Request,
		const FVector& PelletDirection,
		int32 PelletIndex) const;

	bool IsDamageTarget(
		const FHitResult& HitResult,
		const FHitscanDamageTargetSettings& TargetSettings) const;

	bool FindSurfaceExit(
		const FHitResult& EntryHit,
		const FVector& TraceDirection,
		const FHitscanTraceSettings& TraceSettings,
		FVector& OutExitPoint,
		float& OutThicknessCentimeters,
		bool& bOutUsedFallbackThickness) const;

	FHitscanSurfaceRule ResolveSurfaceRule(
		const FHitResult& HitResult,
		const FHitscanShotRequest& Request) const;

	float ResolveFallbackThickness(
		const FHitResult& HitResult,
		const FHitscanTraceSettings& TraceSettings) const;
};
