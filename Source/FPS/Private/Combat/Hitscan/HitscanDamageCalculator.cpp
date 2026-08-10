#include "Combat/Hitscan/HitscanDamageCalculator.h"

#include "Combat/Hitscan/HitZoneDefinition.h"
#include "Curves/CurveFloat.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"

FHitscanImpactSample FHitscanDamageCalculator::MakeImpactSample(
	const FHitResult& HitResult,
	const FVector& ShotDirection,
	const float DistanceMeters,
	const float PenetrationDamageMultiplier,
	const int32 PelletIndex)
{
	FHitscanImpactSample ImpactSample;
	ImpactSample.TargetActor = HitResult.GetActor();
	ImpactSample.ImpactPoint = HitResult.ImpactPoint;
	ImpactSample.ImpactNormal = HitResult.ImpactNormal;
	ImpactSample.BoneName = HitResult.BoneName;
	ImpactSample.ShotDirection = ShotDirection.GetSafeNormal();
	ImpactSample.DistanceMeters = FMath::Max(0.0f, DistanceMeters);
	ImpactSample.PenetrationDamageMultiplier = FMath::Clamp(PenetrationDamageMultiplier, 0.0f, 1.0f);
	ImpactSample.PelletIndex = FMath::Max(0, PelletIndex);
	return ImpactSample;
}

bool FHitscanDamageCalculator::ResolvePelletDamage(
	const FHitscanImpactSample& ImpactSample,
	const FHitscanDamageSettings& DamageSettings,
	FHitscanResolvedPelletDamage& OutResolvedDamage)
{
	OutResolvedDamage = FHitscanResolvedPelletDamage();

	if (!IsValid(ImpactSample.TargetActor.Get())
		|| ImpactSample.PelletIndex < 0
		|| ImpactSample.DistanceMeters < 0.0f
		|| ImpactSample.PenetrationDamageMultiplier < 0.0f
		|| DamageSettings.BaseDamagePerPellet <= 0.0f
		|| ImpactSample.ShotDirection.ContainsNaN()
		|| ImpactSample.ShotDirection.IsNearlyZero())
	{
		return false;
	}

	FGameplayTag HitZoneTag = DamageSettings.DefaultHitZoneTag;
	int32 HitZonePriority = 0;
	bool bHeadshot = false;

	if (IsValid(DamageSettings.HitZoneDefinition.Get()))
	{
		HitZoneTag = DamageSettings.HitZoneDefinition->ResolveHitZone(
			ImpactSample.BoneName,
			DamageSettings.DefaultHitZoneTag);
		HitZonePriority = DamageSettings.HitZoneDefinition->GetHitZonePriority(HitZoneTag);
		bHeadshot = DamageSettings.HitZoneDefinition->IsHeadshotZone(HitZoneTag);
	}
	else if (DamageSettings.HeadshotHitZoneTag.IsValid())
	{
		bHeadshot = HitZoneTag.MatchesTagExact(DamageSettings.HeadshotHitZoneTag);
	}

	const float DistanceMultiplier = CalculateDistanceMultiplier(
		ImpactSample.DistanceMeters,
		DamageSettings.DistanceFalloff);
	const float HitZoneMultiplier = ResolveHitZoneMultiplier(
		HitZoneTag,
		DamageSettings.HitZoneDamageMultipliers);
	const float DamageBeforeHitZoneMultiplier = FMath::Max(
		0.0f,
		DamageSettings.BaseDamagePerPellet
		* DistanceMultiplier
		* FMath::Clamp(ImpactSample.PenetrationDamageMultiplier, 0.0f, 1.0f));
	const float FinalDamage = DamageBeforeHitZoneMultiplier * HitZoneMultiplier;

	if (!FMath::IsFinite(FinalDamage) || FinalDamage <= 0.0f)
	{
		return false;
	}

	OutResolvedDamage.TargetActor = ImpactSample.TargetActor;
	OutResolvedDamage.HitZoneTag = HitZoneTag;
	OutResolvedDamage.DamageBeforeHitZoneMultiplier = DamageBeforeHitZoneMultiplier;
	OutResolvedDamage.FinalDamage = FinalDamage;
	OutResolvedDamage.HitZoneMultiplier = HitZoneMultiplier;
	OutResolvedDamage.DistanceMeters = ImpactSample.DistanceMeters;
	OutResolvedDamage.HitZonePriority = HitZonePriority;
	OutResolvedDamage.PelletIndex = ImpactSample.PelletIndex;
	OutResolvedDamage.bHeadshot = bHeadshot;
	OutResolvedDamage.bBackstab = DamageSettings.bEnableBackstab
		&& IsBackstab(
			ImpactSample.TargetActor.Get(),
			ImpactSample.ShotDirection,
			DamageSettings.BackstabRearConeHalfAngleDegrees);
	return true;
}

float FHitscanDamageCalculator::CalculateDistanceMultiplier(
	const float DistanceMeters,
	const FHitscanDistanceFalloffSettings& FalloffSettings)
{
	const float SafeDistanceMeters = FMath::Max(0.0f, DistanceMeters);
	const float EffectiveRangeMeters = FMath::Max(0.0f, FalloffSettings.EffectiveRangeMeters);
	const float MaximumRangeMeters = FMath::Max(EffectiveRangeMeters, FalloffSettings.MaximumRangeMeters);

	if (IsValid(FalloffSettings.DamageMultiplierCurve.Get()))
	{
		if (EffectiveRangeMeters <= KINDA_SMALL_NUMBER)
		{
			return 1.0f;
		}

		const float CurveInput = SafeDistanceMeters / EffectiveRangeMeters;
		return FMath::Max(0.0f, FalloffSettings.DamageMultiplierCurve->GetFloatValue(CurveInput));
	}

	if (MaximumRangeMeters <= KINDA_SMALL_NUMBER || SafeDistanceMeters <= EffectiveRangeMeters)
	{
		return 1.0f;
	}

	const float MinimumMultiplier = FMath::Clamp(
		FalloffSettings.MinimumDamageMultiplierAtMaximumRange,
		0.0f,
		1.0f);

	if (MaximumRangeMeters <= EffectiveRangeMeters + KINDA_SMALL_NUMBER)
	{
		return MinimumMultiplier;
	}

	const float FalloffAlpha = FMath::Clamp(
		(SafeDistanceMeters - EffectiveRangeMeters)
		/ (MaximumRangeMeters - EffectiveRangeMeters),
		0.0f,
		1.0f);
	return FMath::Lerp(1.0f, MinimumMultiplier, FalloffAlpha);
}

float FHitscanDamageCalculator::ResolveHitZoneMultiplier(
	const FGameplayTag HitZoneTag,
	const TMap<FGameplayTag, float>& HitZoneDamageMultipliers)
{
	if (const float* Multiplier = HitZoneDamageMultipliers.Find(HitZoneTag))
	{
		return FMath::Max(0.0f, *Multiplier);
	}

	return 1.0f;
}

float FHitscanDamageCalculator::RoundDamage(
	const float Damage,
	const FHitscanDamageRoundingSettings& RoundingSettings)
{
	const float SafeDamage = FMath::Max(0.0f, Damage);
	if (RoundingSettings.Mode == EHitscanDamageRoundingMode::None)
	{
		return SafeDamage;
	}

	const float Step = FMath::Max(RoundingSettings.Step, KINDA_SMALL_NUMBER);
	const float ScaledDamage = SafeDamage / Step;

	switch (RoundingSettings.Mode)
	{
	case EHitscanDamageRoundingMode::Nearest:
		return FMath::RoundToFloat(ScaledDamage) * Step;

	case EHitscanDamageRoundingMode::Floor:
		return FMath::FloorToFloat(ScaledDamage) * Step;

	case EHitscanDamageRoundingMode::Ceil:
		return FMath::CeilToFloat(ScaledDamage) * Step;

	case EHitscanDamageRoundingMode::None:
	default:
		return SafeDamage;
	}
}

bool FHitscanDamageCalculator::IsBackstab(
	const AActor* TargetActor,
	const FVector& ShotDirection,
	const float RearConeHalfAngleDegrees)
{
	if (!IsValid(TargetActor) || ShotDirection.IsNearlyZero())
	{
		return false;
	}

	const float ClampedHalfAngle = FMath::Clamp(RearConeHalfAngleDegrees, 0.0f, 180.0f);
	const float MinimumAlignment = FMath::Cos(FMath::DegreesToRadians(ClampedHalfAngle));
	const float Alignment = FVector::DotProduct(
		TargetActor->GetActorForwardVector().GetSafeNormal(),
		ShotDirection.GetSafeNormal());
	return Alignment >= MinimumAlignment;
}
