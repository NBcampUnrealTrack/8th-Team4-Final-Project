#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Combat/Hitscan/HitscanTypes.h"
#include "HitscanSurfaceDefinition.generated.h"

USTRUCT(BlueprintType)
struct FHitscanPhysicalSurfaceRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	FHitscanSurfaceRule SurfaceRule;
};

UCLASS(BlueprintType)
class FPS_API UHitscanSurfaceDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|Penetration")
	FHitscanSurfaceRule ResolveSurfaceRule(TEnumAsByte<EPhysicalSurface> SurfaceType) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration", meta = (AllowPrivateAccess = "true"))
	FHitscanSurfaceRule DefaultSurfaceRule;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration", meta = (AllowPrivateAccess = "true"))
	TArray<FHitscanPhysicalSurfaceRule> SurfaceRules;
};
