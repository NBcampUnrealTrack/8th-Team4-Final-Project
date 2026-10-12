#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class USoundAttenuation;
class USoundBase;

// Presentation only. Callers retain their existing prediction/RPC/notify gates.
namespace CharacterSoundPlayback
{
	// Accepts a pawn or an actor directly owned by that pawn (equipped weapon).
	bool IsLocalPlayer(const AActor* SoundOwner);

	// Returns true when handled by the owning player's non-spatial playback.
	bool TryPlayLocal2D(const AActor* SoundOwner, USoundBase* Sound,
		float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);

	void PlayAtLocation(const AActor* SoundOwner, USoundBase* Sound, const FVector& Location,
		float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);

	void PlayAttached(const AActor* SoundOwner, USoundBase* Sound, USceneComponent* AttachTo,
		USoundAttenuation* AttenuationSettings, float VolumeMultiplier = 1.f,
		float PitchMultiplier = 1.f, FName AttachName = NAME_None);
}
