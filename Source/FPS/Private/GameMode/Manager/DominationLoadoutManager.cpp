#include "GameMode/Manager/DominationLoadoutManager.h"

#include "GameMode/Types/DominationTypes.h"
#include "GameFramework/Pawn.h"
#include "Player/DominationPlayerState.h"
#include "Tags/GameplayTags.h"
#include "Weapon/RogueWeaponSettings.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/WeaponInventoryComponent.h"
#include "Weapon/WeaponLoadoutService.h"

bool UDominationLoadoutManager::HasRequiredWeaponRegistryEntries() const
{
	const URogueWeaponSettings* WeaponSettings = GetDefault<URogueWeaponSettings>();
	return IsValid(WeaponSettings)
		&& WeaponSettings->ResolveWeaponClass(RogueTags::Weapon_Type_Rifle)
		&& WeaponSettings->ResolveWeaponClass(RogueTags::Weapon_Type_Sniper)
		&& WeaponSettings->ResolveWeaponClass(RogueTags::Weapon_Type_Shotgun);
}

bool UDominationLoadoutManager::InitializeCombatClassInventory(
	ADominationPlayerState* PlayerState) const
{
	UWeaponInventoryComponent* WeaponInventory = IsValid(PlayerState)
		? PlayerState->GetWeaponInventory()
		: nullptr;
	const FGameplayTag PrimaryWeaponTag = IsValid(PlayerState)
		? ResolvePrimaryWeaponTag(PlayerState->GetCombatClass())
		: FGameplayTag();
	if (!IsValid(WeaponInventory) || !PrimaryWeaponTag.IsValid())
	{
		return false;
	}

	WeaponInventory->ServerInitializeForNewMatch();
	return WeaponInventory->ServerConfirmPrimaryPurchase(PrimaryWeaponTag);
}

bool UDominationLoadoutManager::ApplyCombatClassLoadout(
	const ADominationPlayerState* PlayerState,
	APawn* Pawn) const
{
	const UWeaponInventoryComponent* WeaponInventory = IsValid(PlayerState)
		? PlayerState->GetWeaponInventory()
		: nullptr;
	if (!IsValid(WeaponInventory) || !IsValid(Pawn))
	{
		return false;
	}

	return RogueWeaponLoadout::ApplyPrimaryWeapon(
		Pawn,
		WeaponInventory->GetEquippedPrimaryTag());
}

FGameplayTag UDominationLoadoutManager::ResolvePrimaryWeaponTag(
	const EDominationCombatClass CombatClass)
{
	switch (CombatClass)
	{
	case EDominationCombatClass::Trooper:
		return RogueTags::Weapon_Type_Rifle;
	case EDominationCombatClass::Ranger:
		return RogueTags::Weapon_Type_Sniper;
	case EDominationCombatClass::Vanguard:
		return RogueTags::Weapon_Type_Shotgun;
	default:
		return FGameplayTag();
	}
}
