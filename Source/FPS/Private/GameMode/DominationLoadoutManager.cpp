#include "GameMode/DominationLoadoutManager.h"

#include "GameMode/DominationTypes.h"
#include "GameFramework/Pawn.h"
#include "Player/DominationPlayerState.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponLoadoutService.h"

bool UDominationLoadoutManager::ApplyCombatClassLoadout(
	const ADominationPlayerState* PlayerState,
	APawn* Pawn) const
{
	if (!IsValid(PlayerState) || !IsValid(Pawn))
	{
		return false;
	}

	FGameplayTag PrimaryWeaponTag;
	switch (PlayerState->GetCombatClass())
	{
	case EDominationCombatClass::Trooper:
		PrimaryWeaponTag = RogueTags::Weapon_Type_Rifle;
		break;
	case EDominationCombatClass::Ranger:
		PrimaryWeaponTag = RogueTags::Weapon_Type_Sniper;
		break;
	case EDominationCombatClass::Vanguard:
		PrimaryWeaponTag = RogueTags::Weapon_Type_Shotgun;
		break;
	default:
		return false;
	}

	return RogueWeaponLoadout::ApplyPrimaryWeapon(Pawn, PrimaryWeaponTag);
}
