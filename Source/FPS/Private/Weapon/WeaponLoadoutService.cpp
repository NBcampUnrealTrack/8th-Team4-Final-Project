#include "Weapon/WeaponLoadoutService.h"

#include "Combat/CombatComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/ShooterPlayerState.h"
#include "Weapon/RogueWeaponSettings.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/WeaponInventoryComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponLoadout, Log, All);

namespace
{
	void SyncEquippedWeaponState(
		AShooterPlayerState& PlayerState,
		const UCombatComponent& Combat)
	{
		const AWeaponBase* EquippedWeapon = Combat.GetCurrentWeapon();

		PlayerState.SyncEquippedWeaponState(
			IsValid(EquippedWeapon) ? EquippedWeapon->GetEquippedStateTag() : FGameplayTag());
	}
}

bool RogueWeaponLoadout::ApplyPrimaryWeapon(APawn* Pawn, const FGameplayTag WeaponTag)
{
	UCombatComponent* Combat = IsValid(Pawn) ? Pawn->FindComponentByClass<UCombatComponent>() : nullptr;
	AShooterPlayerState* PlayerState = IsValid(Pawn) ? Pawn->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (!IsValid(Combat) || !IsValid(PlayerState))
	{
		return false;
	}

	bool bApplied = false;
	if (!WeaponTag.IsValid())
	{
		bApplied = Combat->ReplacePrimaryWeapon(nullptr);
	}
	else
	{
		const TSubclassOf<AWeaponBase> WeaponClass =
			GetDefault<URogueWeaponSettings>()->ResolveWeaponClass(WeaponTag);

		if (!WeaponClass)
		{
			UE_LOG(LogWeaponLoadout, Error,
				TEXT("Weapon %s has no class in the weapon registry. Check Project Settings > Weapon Registry."),
				*WeaponTag.ToString());

			return false;
		}

		bApplied = Combat->ReplacePrimaryWeapon(WeaponClass);
	}

	if (!bApplied)
	{
		return false;
	}

	SyncEquippedWeaponState(*PlayerState, *Combat);
	return true;
}

bool RogueWeaponLoadout::EquipSidearmForRoundStart(APawn* Pawn)
{
	UCombatComponent* Combat = IsValid(Pawn) ? Pawn->FindComponentByClass<UCombatComponent>() : nullptr;
	AShooterPlayerState* PlayerState = IsValid(Pawn) ? Pawn->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (!IsValid(Combat) || !IsValid(PlayerState))
	{
		return false;
	}

	Combat->FinishCycling();

	if (!Combat->SwitchWeaponSlot(EWeaponSlot::Secondary))
	{
		return false;
	}

	SyncEquippedWeaponState(*PlayerState, *Combat);
	return true;
}

bool RogueWeaponLoadout::RestoreConfirmedPrimary(AShooterPlayerState* PlayerState, APawn* Pawn)
{
	const UWeaponInventoryComponent* Inventory =
		IsValid(PlayerState) ? PlayerState->GetWeaponInventory() : nullptr;

	if (!IsValid(Inventory))
	{
		return false;
	}

	return ApplyPrimaryWeapon(Pawn, Inventory->GetEquippedPrimaryTag());
}
