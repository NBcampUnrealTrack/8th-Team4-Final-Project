#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/Hitscan/HitscanShotTypes.h"
#include "HitscanWeaponDefinition.generated.h"

UCLASS(BlueprintType)
class FPS_API UHitscanWeaponDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// 정적 Hitscan 정책은 Definition이, 매 발 변하는 Damage/Range는 Weapon이 소유한다.
	// 둘을 합치는 규칙을 이곳에 모아 WeaponBase가 세부 설정 구조를 직접 조립하지 않게 한다.
	void ConfigureShotRequest(
		FHitscanShotRequest& Request,
		float BaseDamagePerPellet,
		float MaximumRangeCentimeters,
		float FallbackSurfaceThicknessCentimeters,
		float RuntimeDefaultSurfaceHardness) const
	{
		Request.WeaponTag = WeaponTag;

		// TMap(HitZoneDamageMultipliers)을 매 발 복사하지 않도록 실행에 필요한 소형 필드만 복사한다.
		Request.DamageSettings.BaseDamagePerPellet = FMath::Max(0.0f, BaseDamagePerPellet);
		Request.DamageSettings.DistanceFalloff = DamageSettings.DistanceFalloff;
		Request.DamageSettings.HitZoneDefinition = DamageSettings.HitZoneDefinition;
		Request.DamageSettings.DefaultHitZoneTag = DamageSettings.DefaultHitZoneTag;
		Request.DamageSettings.HeadshotHitZoneTag = DamageSettings.HeadshotHitZoneTag;
		Request.DamageSettings.bEnableBackstab = DamageSettings.bEnableBackstab;
		Request.DamageSettings.BackstabRearConeHalfAngleDegrees = DamageSettings.BackstabRearConeHalfAngleDegrees;
		Request.DamageSettings.Rounding = DamageSettings.Rounding;
		Request.HitZoneDamageMultipliers = &DamageSettings.HitZoneDamageMultipliers;

		Request.PenetrationSettings = PenetrationSettings;
		Request.TraceSettings = TraceSettings;
		Request.SurfaceDefinition = SurfaceDefinition.Get();

		const float MaximumRangeMeters = FMath::Max(0.0f, MaximumRangeCentimeters / 100.0f);
		Request.DamageSettings.DistanceFalloff.MaximumRangeMeters = MaximumRangeMeters;
		Request.DamageSettings.DistanceFalloff.EffectiveRangeMeters = FMath::Clamp(
			Request.DamageSettings.DistanceFalloff.EffectiveRangeMeters,
			0.0f,
			MaximumRangeMeters);

		Request.TraceSettings.MaximumRangeCentimeters = FMath::Max(0.0f, MaximumRangeCentimeters);
		Request.TraceSettings.FallbackSurfaceThicknessCentimeters = FMath::Max(
			0.1f,
			FallbackSurfaceThicknessCentimeters);
		if (RuntimeDefaultSurfaceHardness > KINDA_SMALL_NUMBER)
		{
			Request.TraceSettings.DefaultSurfaceRule.Hardness = RuntimeDefaultSurfaceHardness;
		}
	}

	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|Weapon")
	FGameplayTag GetWeaponTag() const { return WeaponTag; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	FGameplayTag WeaponTag;

	// BaseDamagePerPellet/MaximumRange는 Weapon의 실제 발사 값으로 실행 시점에 덮어쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	FHitscanDamageSettings DamageSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	FHitscanPenetrationSettings PenetrationSettings;

	// MaximumRange/FallbackThickness/기본 Hardness는 Weapon의 런타임 값이 최종 우선한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	FHitscanTraceSettings TraceSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHitscanSurfaceDefinition> SurfaceDefinition = nullptr;
};
