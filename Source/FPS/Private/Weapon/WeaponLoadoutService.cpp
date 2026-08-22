#include "Weapon/WeaponLoadoutService.h"

#include "Combat/CombatComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/ShooterPlayerState.h"
#include "Weapon/RogueWeaponSettings.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/WeaponInventoryComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponLoadout, Log, All);

bool RogueWeaponLoadout::ApplyPrimaryWeapon(APawn* Pawn, const EWeaponType WeaponType)
{
	UCombatComponent* Combat = IsValid(Pawn) ? Pawn->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!IsValid(Combat))
	{
		return false;
	}

	if (WeaponType == EWeaponType::None)
	{
		return Combat->ReplacePrimaryWeapon(nullptr);
	}

	const TSubclassOf<AWeaponBase> WeaponClass =
		GetDefault<URogueWeaponSettings>()->ResolveWeaponClass(WeaponType);

	if (!WeaponClass)
	{
		UE_LOG(LogWeaponLoadout, Error,
			TEXT("Weapon %s has no class in the weapon registry. Check Project Settings > Weapon Registry."),
			*UEnum::GetValueAsString(WeaponType));

		return false;
	}

	return Combat->ReplacePrimaryWeapon(WeaponClass);
}

bool RogueWeaponLoadout::RestoreConfirmedPrimary(AShooterPlayerState* PlayerState, APawn* Pawn)
{
	const UWeaponInventoryComponent* Inventory =
		IsValid(PlayerState) ? PlayerState->GetWeaponInventory() : nullptr;

	if (!IsValid(Inventory))
	{
		return false;
	}

	return ApplyPrimaryWeapon(Pawn, Inventory->GetEquippedPrimary());
}
