#include "GameMode/Manager/DominationLoadoutManager.h"

#include "GameMode/Types/DominationTypes.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameFramework/Pawn.h"
#include "Player/DominationPlayerState.h"
#include "Weapon/RogueWeaponSettings.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/WeaponInventoryComponent.h"
#include "Weapon/WeaponLoadoutService.h"

bool UDominationLoadoutManager::HasRequiredWeaponRegistryEntries() const
{
	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	const URogueWeaponSettings* WeaponSettings = GetDefault<URogueWeaponSettings>();
	if (!IsValid(ClassSettings)
		|| !IsValid(WeaponSettings)
		|| ClassSettings->CombatClasses.IsEmpty())
	{
		return false;
	}

	for (const FDominationCombatClassDefinition& Definition : ClassSettings->CombatClasses)
	{
		if (Definition.CombatClass == EDominationCombatClass::Unselected
			|| !Definition.PrimaryWeaponTag.IsValid()
			|| !WeaponSettings->ResolveWeaponClass(Definition.PrimaryWeaponTag))
		{
			return false;
		}
	}

	return true;
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
	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	const FDominationCombatClassDefinition* Definition = IsValid(ClassSettings)
		? ClassSettings->FindDefinition(CombatClass)
		: nullptr;

	return Definition ? Definition->PrimaryWeaponTag : FGameplayTag();
}
