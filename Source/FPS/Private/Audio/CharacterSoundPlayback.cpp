#include "Audio/CharacterSoundPlayback.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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

	const APlayerController* Controller = IsValid(Pawn)
		? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	// Only this process's possessed local player gets non-spatial audio.
	return IsValid(Controller) && Controller->IsLocalController()
		&& Controller->GetLocalPlayer() && Controller->GetPawn() == Pawn;
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
	USceneComponent* AttachTo, USoundAttenuation* AttenuationSettings,
	const float VolumeMultiplier, const float PitchMultiplier, const FName AttachName)
{
	if (!IsValid(SoundOwner) || !IsValid(Sound) || SoundOwner->GetNetMode() == NM_DedicatedServer
		|| TryPlayLocal2D(SoundOwner, Sound, VolumeMultiplier, PitchMultiplier))
	{
		return;
	}

	if (IsValid(AttachTo))
	{
		UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, AttachName, FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset, false, VolumeMultiplier, PitchMultiplier, 0.f, AttenuationSettings);
	}
}
