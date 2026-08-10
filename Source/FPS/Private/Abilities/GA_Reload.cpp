// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_Reload.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

bool UGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
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

	return Weapon->UsesAmmo() && !Weapon->IsReloading() && Weapon->GetCurrentAmmo() < Weapon->GetMagazineSize();
}

void UGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                  const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AWeaponBase* Weapon = GetEquippedWeapon();
	if (!Weapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Weapon->StartReload();
	SendWeaponGameplayEvent(RogueTags::Event_Weapon_ReloadStart);

	if (ReloadMontage && ActorInfo)
	{
		if (UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance())
		{
			AnimInstance->Montage_Play(ReloadMontage);
		}
	}

	// AWeaponBase가 내부 타이머로 재장전을 끝내는 시점과 맞춰서, 같은 시간만큼 대기한 뒤
	// 이 어빌리티도 종료한다. 재장전 도중 사격 차단은 AWeaponBase::IsReloading()이 이미 처리한다.
	if (UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, Weapon->GetReloadDuration()))
	{
		WaitTask->OnFinish.AddDynamic(this, &UGA_Reload::HandleReloadFinished);
		WaitTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
	}
}

void UGA_Reload::HandleReloadFinished()
{
	SendWeaponGameplayEvent(RogueTags::Event_Weapon_ReloadComplete);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}