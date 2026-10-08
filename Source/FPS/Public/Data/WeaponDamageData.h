#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WeaponDamageData.generated.h"

// 무기 대미지 계수
USTRUCT(BlueprintType)
struct FWeaponDamageEntry
{
	GENERATED_BODY()

	// 무기의 기본 피해량
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Damage", meta = (ClampMin = "0.0"))
	float BaseDamagePerPellet = 0.f;

	// 유효 사거리, 감쇠가 적용되지 않는 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Damage|Falloff", meta = (ClampMin = "0.0"))
	float EffectiveRangeMeters = 0.f;

	// 유효 사거리 이후부터 감쇠가 적용되는 최대 사거리, 0이면 감쇠 없음(저격소총)
	// 최대 사거리를 넘는 경우 최소 대미지로 고정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Damage|Falloff", meta = (ClampMin = "0.0"))
	float MaximumRangeMeters = 0.f;

	// 유효 사거리 이후부터 1미터당 감쇠되는 피해량
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Damage|Falloff", meta = (ClampMin = "0.0"))
	float DamageFalloffPerMeter = 0.f;

	// 무기 감쇠가 적용되는 최소 대미지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Damage|Falloff", meta = (ClampMin = "0.0"))
	float MinimumDamage = 0.f;

	// 부위 태그 -> 배율
	// 미등록 태그는 1.0으로 처리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Damage")
	TMap<FGameplayTag, float> HitZoneMultipliers;
};

// 무기 태그를 키로 대미지 계수를 보관
UCLASS(BlueprintType)
class FPS_API UWeaponDamageData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 무기 태그로 조회. false이면 로드된 데이터만 조용히 조회한다.
	static const FWeaponDamageEntry* FindEntry(FGameplayTag WeaponTag, bool bAllowSynchronousLoad = true);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Damage")
	TMap<FGameplayTag, FWeaponDamageEntry> Weapons;
};
