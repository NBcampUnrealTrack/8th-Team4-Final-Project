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
	// 정적 판정 정책을 한 발의 요청에 복사한다.
	void ConfigureShotRequest(
		FHitscanShotRequest& Request,
		float MaximumRangeCentimeters,
		float FallbackSurfaceThicknessCentimeters,
		float RuntimeDefaultSurfaceHardness) const
	{
		Request.WeaponTag = WeaponTag;
		Request.DamageSettings = DamageSettings;
		Request.PenetrationSettings = PenetrationSettings;
		Request.TraceSettings = TraceSettings;
		Request.SurfaceDefinition = SurfaceDefinition.Get();

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
	// WeaponDamageData 조회에 사용하는 Weapon.Type.* 태그.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	FGameplayTag WeaponTag;

	// 기존 DataAsset의 DamageSettings 프로퍼티는 유지하되 절대 대미지 설정은 제거했다.
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
