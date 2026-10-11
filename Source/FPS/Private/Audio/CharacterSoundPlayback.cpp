#include "Audio/CharacterSoundPlayback.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

bool CharacterSoundPlayback::IsLocalPlayer(const AActor* SoundOwner)
{
	if (!IsValid(SoundOwner) || SoundOwner->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const APawn* Pawn = Cast<APawn>(SoundOwner);
	if (!Pawn)
	{
		Pawn = Cast<APawn>(SoundOwner->GetOwner());
	}

	// Server-controlled AI is locally controlled too, but must remain spatial.
	return IsValid(Pawn) && Pawn->IsPlayerControlled() && Pawn->IsLocallyControlled();
}

bool CharacterSoundPlayback::TryPlayLocal2D(const AActor* SoundOwner, USoundBase* Sound,
	const float VolumeMultiplier, const float PitchMultiplier)
{
	if (!IsValid(Sound) || !IsLocalPlayer(SoundOwner))
	{
		return false;
	}

	// Bypass attenuation without modifying the shared sound/ATT asset or treating it as UI audio.
	UGameplayStatics::PlaySound2D(SoundOwner, Sound, VolumeMultiplier, PitchMultiplier,
		0.f, nullptr, SoundOwner, /*bIsUISound*/ false);
	return true;
}

void CharacterSoundPlayback::PlayAtLocation(const AActor* SoundOwner, USoundBase* Sound,
	const FVector& Location, const float VolumeMultiplier, const float PitchMultiplier)
{
	if (!IsValid(SoundOwner) || !IsValid(Sound) || SoundOwner->GetNetMode() == NM_DedicatedServer
		|| TryPlayLocal2D(SoundOwner, Sound, VolumeMultiplier, PitchMultiplier))
	{
		return;
	}

	// Observers keep the sound asset's attenuation, volume and pitch.
	UGameplayStatics::PlaySoundAtLocation(SoundOwner, Sound, Location, VolumeMultiplier, PitchMultiplier);
}

void CharacterSoundPlayback::PlayAttached(const AActor* SoundOwner, USoundBase* Sound,
	USceneComponent* AttachTo, USoundAttenuation* AttenuationSettings)
{
	if (!IsValid(SoundOwner) || !IsValid(Sound) || SoundOwner->GetNetMode() == NM_DedicatedServer
		|| TryPlayLocal2D(SoundOwner, Sound))
	{
		return;
	}

	if (IsValid(AttachTo))
	{
		UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset, false, 1.f, 1.f, 0.f, AttenuationSettings);
	}
}
