#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponEffectsComponent.generated.h"

class AWeaponBase;
class UCameraShakeBase;
class USoundBase;
class USoundAttenuation;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UWeaponEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponEffectsComponent();

	void PlayFireCameraShake() const;
	void PlayFireSound() const;
	void PlayEmptyClipSound();

protected:
	AWeaponBase* GetWeaponOwner() const;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayEmptyClipSound();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundBase> EmptyClipSound;

	// Separate from the shared sound asset so each weapon can have its own range.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundAttenuation> FireAttenuation;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundAttenuation> ActionAttenuation;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Camera")
	TSubclassOf<UCameraShakeBase> FireCameraShake;
};
