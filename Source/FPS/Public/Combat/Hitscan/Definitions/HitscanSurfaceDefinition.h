#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"
#include "HitscanSurfaceDefinition.generated.h"

class USoundBase;
class UParticleSystem;

USTRUCT(BlueprintType)
struct FHitscanPhysicalSurfaceRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	FHitscanSurfaceRule SurfaceRule;

	// 이 표면에 맞았을 때 재생할 임팩트 사운드(예: 사람=살 맞는 소리, 콘크리트/유리=벽 맞는 소리).
	// 비어있으면 DefaultImpactSound로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Impact")
	TObjectPtr<USoundBase> ImpactSound;

	// 여러 사운드를 등록하면 이 중 하나를 랜덤 재생한다(예: 사람 바디샷음 여러 종).
	// 비어있으면 ImpactSound(단일)로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Impact")
	TArray<TObjectPtr<USoundBase>> ImpactSoundVariants;

	// 헤드샷일 때 재생할 고정 사운드. 비어있으면 ImpactSound/ImpactSoundVariants로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Impact")
	TObjectPtr<USoundBase> HeadshotImpactSound;

	// 이 표면에 맞았을 때 스폰할 임팩트 파티클(예: 사람=혈흔, 콘크리트=먼지). 비어있으면 DefaultImpactParticle로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Impact")
	TObjectPtr<UParticleSystem> ImpactParticle;

	// 여러 파티클을 등록하면 이 중 하나를 랜덤 스폰한다(예: 혈흔 파티클 여러 종). 비어있으면 ImpactParticle(단일)로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Impact")
	TArray<TObjectPtr<UParticleSystem>> ImpactParticleVariants;

	// 헤드샷일 때 스폰할 고정 파티클. 비어있으면 ImpactParticle/ImpactParticleVariants로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Impact")
	TObjectPtr<UParticleSystem> HeadshotImpactParticle;
};

UCLASS(BlueprintType)
class FPS_API UHitscanSurfaceDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|Penetration")
	FHitscanSurfaceRule ResolveSurfaceRule(TEnumAsByte<EPhysicalSurface> SurfaceType) const;

	// SurfaceRules에 일치하는 표면이 있으면 그 ImpactSound를, 없거나 비어있으면 DefaultImpactSound를 반환한다.
	// bHeadshot이 true면 HeadshotImpactSound(고정)를 우선하고, 그 외에는 ImpactSoundVariants 중 랜덤 하나를 반환한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|Impact")
	USoundBase* ResolveImpactSound(TEnumAsByte<EPhysicalSurface> SurfaceType, bool bHeadshot = false) const;

	// SurfaceRules에 일치하는 표면이 있으면 그 ImpactParticle을, 없거나 비어있으면 DefaultImpactParticle을 반환한다.
	// bHeadshot이 true면 HeadshotImpactParticle(고정)을 우선하고, 그 외에는 ImpactParticleVariants 중 랜덤 하나를 반환한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|Impact")
	UParticleSystem* ResolveImpactParticle(TEnumAsByte<EPhysicalSurface> SurfaceType, bool bHeadshot = false) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration", meta = (AllowPrivateAccess = "true"))
	FHitscanSurfaceRule DefaultSurfaceRule;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration", meta = (AllowPrivateAccess = "true"))
	TArray<FHitscanPhysicalSurfaceRule> SurfaceRules;

	// SurfaceRules에 표면별 ImpactSound가 없을 때 쓰는 기본 임팩트 사운드.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Impact", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> DefaultImpactSound;

	// SurfaceRules에 표면별 ImpactParticle이 없을 때 쓰는 기본 임팩트 파티클.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Impact", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UParticleSystem> DefaultImpactParticle;
};
