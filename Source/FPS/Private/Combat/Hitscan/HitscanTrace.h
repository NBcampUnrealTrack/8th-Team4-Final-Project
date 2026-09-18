#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"

class UWorld;
class AActor;
struct FCollisionQueryParams;
struct FHitResult;
struct FHitscanPenetrationState;

class FHitscanTrace
{
public:
	static bool ResolveImpacts(
		UWorld* World,
		const FHitscanShotRequest& Request,
		TArray<FHitscanImpactSample>& OutImpactSamples,
		TArray<FHitscanImpactEffectData>& OutImpactEffects);

private:
	static bool IsPotentialDamageTargetActor(
		AActor* TargetActor,
		const FHitscanDamageTargetSettings& TargetSettings);
	static bool IsPotentialDamageTarget(
		const FHitResult& HitResult,
		const FHitscanDamageTargetSettings& TargetSettings);
	static FGameplayTag ResolveHitZoneTag(
		const FHitResult& HitResult,
		const FHitscanShotRequest& Request,
		FName BoneName);
	static bool FindSurfaceExit(
		const FHitResult& EntryHit,
		const FVector& TraceDirection,
		const FHitscanTraceSettings& TraceSettings,
		float RemainingRangeAfterEntryCentimeters,
		FVector& OutExitPoint);
	static FHitscanSurfaceRule ResolveSurfaceRule(
		const FHitResult& HitResult,
		const FHitscanShotRequest& Request);
	static bool TraceNextBlockingHit(
		UWorld* World,
		const FHitscanShotRequest& Request,
		const FCollisionQueryParams& QueryParams,
		const FVector& TraceStart,
		const FVector& TraceDirection,
		float RemainingRangeCentimeters,
		FHitResult& OutHitResult);
	static bool TryBuildImpactSample(
		const FHitResult& HitResult,
		const FVector& TraceDirection,
		const FHitscanPenetrationState& PenetrationState,
		const FHitscanShotRequest& Request,
		int32 PelletIndex,
		FHitscanImpactSample& OutImpactSample);
	static FHitscanImpactEffectData BuildImpactEffectData(
		const FHitResult& HitResult,
		const FHitscanShotRequest& Request,
		int32 PelletIndex);
	// PhysMaterial 기반 표면 판정 폴백. Mutable/MetaHuman 캐릭터는 런타임에 머티리얼이 재생성돼
	// 소스 에셋에 지정한 Phys Material이 그대로 안 이어질 수 있으므로, ResolveHitZoneTag와 동일하게
	// "맞은 액터가 URogueDamageableTarget을 구현하는가"를 먼저 확인해 Character 표면으로 강제한다.
	static TEnumAsByte<EPhysicalSurface> ResolveImpactSurfaceType(const FHitResult& HitResult);
	static bool TryAdvanceThroughSurface(
		const FHitResult& EntryHit,
		const FVector& TraceDirection,
		float RemainingRangeAfterEntryCentimeters,
		const FHitscanShotRequest& Request,
		FHitscanPenetrationState& InOutPenetrationState,
		FVector& OutNextTraceStart);
	static bool ResolveCurrentPellet(
		UWorld* World,
		const FHitscanShotRequest& Request,
		const FVector& PelletDirection,
		int32 PelletIndex,
		FHitscanImpactSample& OutImpactSample,
		FHitscanImpactEffectData& OutImpactEffect,
		bool& bOutHasImpactEffect);
};
