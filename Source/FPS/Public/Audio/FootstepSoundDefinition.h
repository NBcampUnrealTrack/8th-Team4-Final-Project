#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "FootstepSoundDefinition.generated.h"

class USoundBase;

USTRUCT(BlueprintType)
struct FFootstepSurfaceSound
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footstep")
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footstep")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footstep", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footstep", meta = (ClampMin = "0.0"))
	float PitchMultiplier = 1.0f;
};

UCLASS(BlueprintType)
class FPS_API UFootstepSoundDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	const FFootstepSurfaceSound* ResolveFootstepSound(TEnumAsByte<EPhysicalSurface> SurfaceType) const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Footstep")
	TArray<FFootstepSurfaceSound> SurfaceSounds;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep")
	FFootstepSurfaceSound DefaultSound;
};
