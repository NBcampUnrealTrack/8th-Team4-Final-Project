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
	FGameplayTag GetWeaponTag() const { return WeaponTag; }
	const FHitscanDamageSettings& GetDamageSettings() const { return DamageSettings; }
	const FHitscanPenetrationSettings& GetPenetrationSettings() const { return PenetrationSettings; }
	const FHitscanTraceSettings& GetTraceSettings() const { return TraceSettings; }
	UHitscanSurfaceDefinition* GetSurfaceDefinition() const { return SurfaceDefinition.Get(); }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	FGameplayTag WeaponTag;

	// BaseDamagePerPellet과 MaximumRange는 AWeaponBase의 현재 발사 값으로 실행 시점에 덮어쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	FHitscanDamageSettings DamageSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	FHitscanPenetrationSettings PenetrationSettings;

	// MaximumRange, FallbackThickness, 기본 Hardness는 기존 AWeaponBase 설정을 유지하기 위해 실행 시점에 덮어쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	FHitscanTraceSettings TraceSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHitscanSurfaceDefinition> SurfaceDefinition = nullptr;
};
