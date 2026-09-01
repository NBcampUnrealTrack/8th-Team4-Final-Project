#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"
#include "HitscanSurfaceDefinition.generated.h"

class USoundBase;

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
};

UCLASS(BlueprintType)
class FPS_API UHitscanSurfaceDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|Penetration")
	FHitscanSurfaceRule ResolveSurfaceRule(TEnumAsByte<EPhysicalSurface> SurfaceType) const;

	// SurfaceRules에 일치하는 표면이 있으면 그 ImpactSound를, 없거나 비어있으면 DefaultImpactSound를 반환한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|Impact")
	USoundBase* ResolveImpactSound(TEnumAsByte<EPhysicalSurface> SurfaceType) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration", meta = (AllowPrivateAccess = "true"))
	FHitscanSurfaceRule DefaultSurfaceRule;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration", meta = (AllowPrivateAccess = "true"))
	TArray<FHitscanPhysicalSurfaceRule> SurfaceRules;

	// SurfaceRules에 표면별 ImpactSound가 없을 때 쓰는 기본 임팩트 사운드.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Impact", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> DefaultImpactSound;
};
