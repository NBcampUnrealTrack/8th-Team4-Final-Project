#include "Settings/ShooterAudioSettings.h"
#include "Sound/SoundClass.h"

FName UShooterAudioSettings::GetCategoryName() const
{
	return TEXT("Game");
}

USoundClass* UShooterAudioSettings::LoadSoundClass(const TSoftObjectPtr<USoundClass>& SoundClassReference)
{
	USoundClass* SoundClass = SoundClassReference.LoadSynchronous();
	if (IsValid(SoundClass))
	{
		LoadedSoundClasses.AddUnique(SoundClass);
	}
	return SoundClass;
}

const TArray<TSoftObjectPtr<USoundBase>>& UShooterAudioSettings::ResolvePainSoundPool(
	const EDominationCombatClass CombatClass) const
{
	const TArray<TSoftObjectPtr<USoundBase>>* Pool = &PainSounds_Default;
	switch (CombatClass)
	{
	case EDominationCombatClass::Trooper:
		Pool = &PainSounds_Trooper;
		break;
	case EDominationCombatClass::Ranger:
		Pool = &PainSounds_Ranger;
		break;
	case EDominationCombatClass::Vanguard:
		Pool = &PainSounds_Vanguard;
		break;
	default:
		break;
	}

	return Pool->IsEmpty() ? PainSounds_Default : *Pool;
}
