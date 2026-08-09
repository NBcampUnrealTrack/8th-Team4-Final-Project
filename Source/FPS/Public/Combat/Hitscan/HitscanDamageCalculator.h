#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/HitscanTypes.h"

struct FHitResult;

class FPS_API FHitscanDamageCalculator final
{
public:
	static FHitscanImpactSample MakeImpactSample(
		const FHitResult& HitResult,
		const FVector& ShotDirection,
		float DistanceMeters,
		float PenetrationDamageMultiplier,
		int32 PelletIndex);

	static bool ResolvePelletDamage(
		const FHitscanImpactSample& ImpactSample,
		const FHitscanDamageSettings& DamageSettings,
		FHitscanResolvedPelletDamage& OutResolvedDamage);

	static float CalculateDistanceMultiplier(
		float DistanceMeters,
		const FHitscanDistanceFalloffSettings& FalloffSettings);

	static float ResolveHitZoneMultiplier(
		FGameplayTag HitZoneTag,
		const TMap<FGameplayTag, float>& HitZoneDamageMultipliers);

	static float RoundDamage(
		float Damage,
		const FHitscanDamageRoundingSettings& RoundingSettings);

	static bool IsBackstab(
		const AActor* TargetActor,
		const FVector& ShotDirection,
		float RearConeHalfAngleDegrees);
};
