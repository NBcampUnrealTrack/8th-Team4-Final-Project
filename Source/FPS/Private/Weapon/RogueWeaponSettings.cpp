#include "Weapon/RogueWeaponSettings.h"

#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

URogueWeaponSettings::URogueWeaponSettings()
{
	// 클래스는 프로젝트 설정에서 지정한다.
	PrimaryWeapons =
	{
		{RogueTags::Weapon_Type_Rifle, nullptr},
		{RogueTags::Weapon_Type_Sniper, nullptr},
		{RogueTags::Weapon_Type_Shotgun, nullptr}
	};
}

TSubclassOf<AWeaponBase> URogueWeaponSettings::ResolveWeaponClass(const FGameplayTag WeaponTag) const
{
	const FRoguePrimaryWeaponEntry* Entry = PrimaryWeapons.FindByPredicate(
		[WeaponTag](const FRoguePrimaryWeaponEntry& Candidate)
		{
			return Candidate.WeaponTag == WeaponTag;
		});

	if (!Entry || Entry->WeaponClass.IsNull())
	{
		return nullptr;
	}

	return Entry->WeaponClass.LoadSynchronous();
}
