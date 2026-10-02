#include "Settings/ShooterAudioSettings.h"

FName UShooterAudioSettings::GetCategoryName() const
{
	return TEXT("Game");
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

const TArray<TSoftObjectPtr<USoundBase>>& UShooterAudioSettings::ResolveExhaustedBreathSoundPool(
	const EDominationCombatClass CombatClass) const
{
	const TArray<TSoftObjectPtr<USoundBase>>* Pool = &ExhaustedBreathSounds_Default;
	switch (CombatClass)
	{
	case EDominationCombatClass::Trooper:
		Pool = &ExhaustedBreathSounds_Trooper;
		break;
	case EDominationCombatClass::Ranger:
		Pool = &ExhaustedBreathSounds_Ranger;
		break;
	case EDominationCombatClass::Vanguard:
		Pool = &ExhaustedBreathSounds_Vanguard;
		break;
	default:
		break;
	}

	return Pool->IsEmpty() ? ExhaustedBreathSounds_Default : *Pool;
}
