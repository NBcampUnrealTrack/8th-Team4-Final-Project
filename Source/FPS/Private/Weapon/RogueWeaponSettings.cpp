#include "Weapon/RogueWeaponSettings.h"

#include "Weapon/WeaponBase.h"

URogueWeaponSettings::URogueWeaponSettings()
{
	// 클래스는 프로젝트 설정에서 지정한다.
	PrimaryWeapons =
	{
		{EWeaponType::AR,      nullptr},
		{EWeaponType::Sniper,  nullptr},
		{EWeaponType::Shotgun, nullptr}
	};
}

TSubclassOf<AWeaponBase> URogueWeaponSettings::ResolveWeaponClass(const EWeaponType WeaponType) const
{
	const FRoguePrimaryWeaponEntry* Entry = PrimaryWeapons.FindByPredicate(
		[WeaponType](const FRoguePrimaryWeaponEntry& Candidate)
		{
			return Candidate.WeaponType == WeaponType;
		});

	if (!Entry || Entry->WeaponClass.IsNull())
	{
		return nullptr;
	}

	return Entry->WeaponClass.LoadSynchronous();
}
