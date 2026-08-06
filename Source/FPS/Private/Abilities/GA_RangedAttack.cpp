// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_RangedAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/RPMUtils.h"
#include "Animation/AnimInstance.h"
#include "Weapon/WeaponBase.h"

bool UGA_RangedAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
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

	// RPM 쿨다운은 AWeaponBase::CanFireNow() 내부에서 처리하므로 여기서는 중복 체크하지 않는다.
	return !Weapon->IsReloading() && Weapon->GetCurrentAmmo() > 0;
}

void UGA_RangedAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWeaponBase* Weapon = GetEquippedWeapon();
	if (!Weapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// AWeaponBase가 유일하게 GAS 영향을 받는 경로: GA_RangedAttack이 대신 계산해서 꽂아준다.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const float FinalRPM = URPMUtils::GetFinalRPM(ASC, Weapon->GetBaseRPM());
		Weapon->SetEffectiveRPM(FinalRPM);
	}

	if (FireMontage && ActorInfo)
	{
		if (UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance())
		{
			AnimInstance->Montage_Play(FireMontage);
		}
	}

	const bool bFired = Weapon->Fire();

	if (bFired)
	{
		SendWeaponGameplayEvent(TEXT("Event.Weapon.Fired"));
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}