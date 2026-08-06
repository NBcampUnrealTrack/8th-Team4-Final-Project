// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_MeleeAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/RPMUtils.h"
#include "Animation/AnimInstance.h"
#include "Weapon/WeaponBase.h"

bool UGA_MeleeAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
                                          FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AWeaponBase* Weapon = GetEquippedWeapon();
	if (!Weapon)
	{
		return false;
	}

	// 근접무기는 탄약이 없으므로(bUsesAmmo=false) 재장전 여부만 확인한다.
	return !Weapon->IsReloading();
}

void UGA_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                       const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWeaponBase* Weapon = GetEquippedWeapon();
	if (!Weapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const float FinalRPM = URPMUtils::GetFinalRPM(ASC, Weapon->GetBaseRPM());
		Weapon->SetEffectiveRPM(FinalRPM);
	}

	if (AttackMontage && ActorInfo)
	{
		if (UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance())
		{
			AnimInstance->Montage_Play(AttackMontage);
		}
	}

	const bool bAttacked = Weapon->Fire(bIsHeavyAttack);

	if (bAttacked)
	{
		SendWeaponGameplayEvent(TEXT("Event.Weapon.Fired"));
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}