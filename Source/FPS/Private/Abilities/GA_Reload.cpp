// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_Reload.h"

#include "GAS/RogueAbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "Combat/CombatComponent.h"
#include "Data/WeaponData.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

UGA_Reload::UGA_Reload()
{
	ActivationOwnedTags.AddTag(RogueTags::State_Combat_IsReloading);
	SetAssetTags(FGameplayTagContainer(RogueTags::State_Combat_IsReloading));
}

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

	return Weapon->CanReload();
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

	// 재장전은 현재 무기의 ADS 능력과 눌림 상태를 함께 정리한다.
	if (URogueAbilitySystemComponent* ASC = Cast<URogueAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		ASC->CancelADSAbilityForWeapon(Weapon);
	}
	if (UCombatComponent* Combat = GetCombatComponent())
	{
		Combat->SetAiming(false, Weapon);
	}

	// 재장전 시간의 기준값은 실제 리로드 몽타주 길이다(무기 BP의 ReloadDuration 필드는 몽타주가
	// 없는 경우에만 쓰는 폴백). 이렇게 해야 몽타주 재생이 끝나는 시점과 Fire 차단이 풀리는 시점이
	// 항상 일치한다 - 어긋나면 애니메이션 도중에 사격이 가능해지는 문제가 생긴다.
	float BaseReloadDuration = Weapon->GetReloadDuration();
	if (const UCombatComponent* CombatComponent = GetCombatComponent())
	{
		if (const UWeaponData* WeaponData = CombatComponent->GetWeaponData())
		{
			const float MontageDuration = WeaponData->GetReloadDuration(Weapon->GetWeaponTag());
			if (MontageDuration > 0.f)
			{
				BaseReloadDuration = MontageDuration;
			}
		}
	}

	// StartReload()가 내부 타이머를 세우기 전에 최종값을 먼저 배선해야, WeaponBase의 실제 재장전
	// 시간/탄창 크기와 아래 WaitDelay가 같은 값을 보고 동기화된다.
	float FinalReloadDuration = BaseReloadDuration;
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		const int32 FinalMagazineSize = URogueAugmentStatResolver::GetFinalMagazineSize(Avatar, Weapon->GetMagazineSize());
		Weapon->SetEffectiveMagazineSize(FinalMagazineSize);

		FinalReloadDuration = URogueAugmentStatResolver::GetFinalReloadTime(Avatar, BaseReloadDuration);
		Weapon->SetEffectiveReloadDuration(FinalReloadDuration);
	}

	ReloadingWeapon = Weapon;
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
	// 이 어빌리티도 종료한다(위에서 배선한 것과 같은 FinalReloadDuration을 써야 서로 어긋나지 않는다).
	// 재장전 도중 사격 차단은 AWeaponBase::IsReloading()이 이미 처리한다.
	if (UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, FinalReloadDuration))
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

void UGA_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bWasCancelled && ReloadingWeapon.IsValid()) ReloadingWeapon->CancelReload();
	ReloadingWeapon.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
