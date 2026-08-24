// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_SwitchWeapon.h"

#include "Combat/CombatComponent.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

bool UGA_SwitchWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                           const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
                                           FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UCombatComponent* CombatComponent = GetCombatComponent();
	return IsValid(CombatComponent) && IsValid(CombatComponent->GetWeaponInSlot(TargetWeaponSlot));
}

void UGA_SwitchWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UCombatComponent* CombatComponent = GetCombatComponent();
	if (!CombatComponent)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const AWeaponBase* PreviousWeapon = CombatComponent->GetCurrentWeapon();
	const bool bSwitched = CombatComponent->SwitchWeaponSlot(TargetWeaponSlot);

	if (bSwitched && CombatComponent->GetCurrentWeapon() != PreviousWeapon)
	{
		SendWeaponGameplayEvent(RogueTags::Event_Weapon_Swap);

		if (URogueAbilitySystemComponent* RogueASC = Cast<URogueAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
		{
			RogueASC->SetEquippedWeaponType(CombatComponent->GetCurrentWeapon()->GetEquippedStateTag());
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
