#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "FootstepComponent.generated.h"

class UFootstepSoundDefinition;
class USkeletalMeshComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPS_API UFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFootstepComponent();

	void PlayFootstep(USkeletalMeshComponent* SourceMesh, FName FootSocketName);

private:
	UFUNCTION(Server, Unreliable)
	void ServerPlayFootstep(FName FootSocketName);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFootstep(FVector_NetQuantize FootstepLocation, EPhysicalSurface SurfaceType);

	bool BuildFootstepData(
		USkeletalMeshComponent* SourceMesh,
		FName FootSocketName,
		FVector& OutFootstepLocation,
		TEnumAsByte<EPhysicalSurface>& OutSurfaceType) const;

	void PlayFootstepSound(FVector FootstepLocation, TEnumAsByte<EPhysicalSurface> SurfaceType) const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Footstep")
	TObjectPtr<UFootstepSoundDefinition> FootstepSoundDefinition;
};
