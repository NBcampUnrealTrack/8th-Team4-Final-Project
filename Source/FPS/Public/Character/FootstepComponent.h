#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameMode/Types/DominationTypes.h"
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

	void PlayFootstep(
		USkeletalMeshComponent* SourceMesh,
		FName FootSocketName,
		float VolumeMultiplier = 1.0f,
		float PitchMultiplier = 1.0f);
	void PlayJumpSound();
	void PlayCrouchSound();
	void PlayStandSound();

private:
	UFUNCTION(Server, Unreliable)
	void ServerPlayFootstep(FName FootSocketName, float VolumeMultiplier, float PitchMultiplier);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFootstep(
		FVector_NetQuantize FootstepLocation,
		EPhysicalSurface SurfaceType,
		float VolumeMultiplier,
		float PitchMultiplier);

	UFUNCTION(Server, Unreliable)
	void ServerPlayJumpSound();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayJumpSound(EDominationCombatClass CombatClass);

	UFUNCTION(Server, Unreliable)
	void ServerPlayStanceSound(bool bEnteringCrouch);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayStanceSound(bool bEnteringCrouch);

	bool BuildFootstepData(
		USkeletalMeshComponent* SourceMesh,
		FName FootSocketName,
		FVector& OutFootstepLocation,
		TEnumAsByte<EPhysicalSurface>& OutSurfaceType) const;

	void PlayFootstepSound(
		FVector FootstepLocation,
		TEnumAsByte<EPhysicalSurface> SurfaceType,
		float VolumeMultiplier = 1.0f,
		float PitchMultiplier = 1.0f) const;
	EDominationCombatClass ResolveCombatClass() const;
	void PlayJumpSoundAtOwner(EDominationCombatClass CombatClass) const;
	void PlayStanceSound(bool bEnteringCrouch);
	void PlayStanceSoundAtOwner(bool bEnteringCrouch) const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Footstep")
	TObjectPtr<UFootstepSoundDefinition> FootstepSoundDefinition;
};
