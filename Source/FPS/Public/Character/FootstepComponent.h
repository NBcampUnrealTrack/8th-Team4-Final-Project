#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "FootstepComponent.generated.h"

class UFootstepSoundDefinition;
class USkeletalMeshComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPS_API UFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFootstepComponent();

	void PlayFootstep(USkeletalMeshComponent* SourceMesh, FName FootSocketName) const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Footstep")
	TObjectPtr<UFootstepSoundDefinition> FootstepSoundDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep", meta = (ClampMin = "0.0"))
	float TraceDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep", meta = (ClampMin = "0.0"))
	float BaseVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep", meta = (ClampMin = "0.0"))
	float BasePitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep")
	bool bSkipWhenFalling = true;
};
