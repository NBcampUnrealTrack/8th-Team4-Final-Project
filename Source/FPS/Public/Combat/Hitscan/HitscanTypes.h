#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "HitscanTypes.generated.h"

class UHitscanHitZoneDefinition;

// 기존 HitscanWeaponDefinition DataAsset의 직렬화 경계를 유지하기 위해 이름은 보존한다.
// 신규 대미지 파이프라인에서는 절대 대미지를 계산하지 않고 부위/후방 판정 정책만 가진다.
USTRUCT(BlueprintType)
struct FHitscanDamageSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Hit")
	TObjectPtr<UHitscanHitZoneDefinition> HitZoneDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Hit")
	FGameplayTag DefaultHitZoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Hit")
	bool bEnableBackstab = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Hit", meta = (EditCondition = "bEnableBackstab", ClampMin = "0.0", ClampMax = "180.0"))
	float BackstabRearConeHalfAngleDegrees = 45.0f;
};

USTRUCT(BlueprintType)
struct FHitscanSurfaceRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0"))
	float Hardness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0"))
	float PenetrationCostPerCentimeter = 0.0f;

	// 표면 1cm 통과 후 남는 관통 보존 계수. 절대 대미지는 계산하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageRetentionPerCentimeter = 1.0f;
};

USTRUCT(BlueprintType)
struct FHitscanPenetrationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0", ClampMax = "64"))
	int32 MaximumPenetrationCount = 0;
};
