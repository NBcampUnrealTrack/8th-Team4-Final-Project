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

	float ResolveFootstepVolumeMultiplier(float VolumeMultiplier) const;
	bool ShouldEmitMovementSound() const;
	void ReportFootstepNoise(const FVector& FootstepLocation) const;

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

	UPROPERTY(EditDefaultsOnly, Category = "Footstep", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrouchFootstepVolumeMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep|Trace", meta = (ClampMin = "1.0", Units = "cm"))
	float FootstepTraceDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep|AI Hearing", meta = (ClampMin = "0.0", Units = "cm"))
	float FootstepNoiseMaxRange = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep|AI Hearing", meta = (ClampMin = "0.0", Units = "cm/s"))
	float WalkingNoiseSpeedThreshold = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep|AI Hearing", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RunningNoiseSpeedThreshold = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep|AI Hearing", meta = (ClampMin = "0.0"))
	float SlowFootstepNoiseLoudness = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep|AI Hearing", meta = (ClampMin = "0.0"))
	float WalkingFootstepNoiseLoudness = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep|AI Hearing", meta = (ClampMin = "0.0"))
	float RunningFootstepNoiseLoudness = 1.0f;
};
