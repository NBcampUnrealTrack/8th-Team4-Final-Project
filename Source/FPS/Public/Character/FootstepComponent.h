#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
};
