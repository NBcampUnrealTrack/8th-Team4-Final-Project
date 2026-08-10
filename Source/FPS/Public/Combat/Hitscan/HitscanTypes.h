#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "HitscanTypes.generated.h"

class UCurveFloat;
class UHitscanHitZoneDefinition;

UENUM(BlueprintType)
enum class EHitscanDamageRoundingMode : uint8
{
	None,
	Nearest,
	Floor,
	Ceil
};

USTRUCT(BlueprintType)
struct FHitscanDistanceFalloffSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (ClampMin = "0.0"))
	float EffectiveRangeMeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (ClampMin = "0.0"))
	float MaximumRangeMeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumDamageMultiplierAtMaximumRange = 1.0f;

	// 입력은 Distance / EffectiveRange. 결과는 [MinimumDamageMultiplierAtMaximumRange, 1]로 제한한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	TObjectPtr<UCurveFloat> DamageMultiplierCurve = nullptr;
};

USTRUCT(BlueprintType)
struct FHitscanDamageRoundingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	EHitscanDamageRoundingMode Mode = EHitscanDamageRoundingMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (ClampMin = "0.0001"))
	float Step = 0.01f;
};

USTRUCT(BlueprintType)
struct FHitscanDamageSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (ClampMin = "0.0"))
	float BaseDamagePerPellet = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	FHitscanDistanceFalloffSettings DistanceFalloff;

	// Resolver는 Bone -> HitZone 판정만 한다. 부위 배율의 실제 곱셈은 ExecCalc_Damage가 담당한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	TObjectPtr<UHitscanHitZoneDefinition> HitZoneDefinition = nullptr;

	// 무기 고유 부위 계수. Resolver는 해당 계수를 DamageRequest 메타데이터로 전달만 하고 곱하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	TMap<FGameplayTag, float> HitZoneDamageMultipliers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	FGameplayTag DefaultHitZoneTag;

	// HitZoneDefinition이 없을 때 사용할 레거시 헤드 판정 태그.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	FGameplayTag HeadshotHitZoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	bool bEnableBackstab = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (EditCondition = "bEnableBackstab", ClampMin = "0.0", ClampMax = "180.0"))
	float BackstabRearConeHalfAngleDegrees = 45.0f;

	// 거리/관통으로 계산된 대상별 원본 피해를 GAS 전달 전에 정규화하는 규칙.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	FHitscanDamageRoundingSettings Rounding;
};

USTRUCT(BlueprintType)
struct FHitscanSurfaceRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0"))
	float Hardness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0"))
	float PenetrationCostPerCentimeter = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageRetentionPerCentimeter = 1.0f;
};

USTRUCT(BlueprintType)
struct FHitscanPenetrationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0", ClampMax = "64"))
	int32 MaximumPenetrationCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumDamageMultiplierToContinue = 0.0f;
};
