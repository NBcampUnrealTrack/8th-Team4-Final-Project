// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_SwitchWeapon.h"

#include "GAS/RogueAbilitySystemComponent.h"
#include "Combat/CombatComponent.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

UGA_SwitchWeapon::UGA_SwitchWeapon()
{
	ActivationBlockedTags.RemoveTag(RogueTags::State_Combat_UtilityHeld);
	CancelAbilitiesWithTag.AddTag(RogueTags::Augment_Ability);
	// 재장전 도중 스왑되면 재장전 어빌리티(GA_Reload)를 즉시 취소한다 - 실제 탄약 상태 정리는
	// CombatComponent::SwitchWeaponSlot()이 AWeaponBase::CancelReload()로 직접 처리하므로, 여기서는
	// GAS 어빌리티 인스턴스(WaitDelay 태스크 등)가 스왑 이후까지 남아있지 않도록 정리하는 역할이다.
	CancelAbilitiesWithTag.AddTag(RogueTags::State_Combat_IsReloading);
}

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

	AWeaponBase* PreviousWeapon = CombatComponent->GetCurrentWeapon();
	if (CombatComponent->GetWeaponInSlot(TargetWeaponSlot) != PreviousWeapon)
	{
		if (URogueAbilitySystemComponent* ASC = Cast<URogueAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
		{
			ASC->CancelADSAbilityForWeapon(PreviousWeapon);
		}
		CombatComponent->SetAiming(false, PreviousWeapon);
	}
	const bool bSwitched = CombatComponent->SwitchWeaponSlot(TargetWeaponSlot);

	if (bSwitched && CombatComponent->GetCurrentWeapon() != PreviousWeapon)
	{
		SendWeaponGameplayEvent(RogueTags::Event_Weapon_Swap);

		// SetEquippedWeaponType()은 로컬 태그 세팅뿐 자체 리플리케이션이 없다(비복제 Loose Tag).
		// 이 if 블록은 SwitchWeaponSlot()의 authority 게이트 때문에 서버 실행에서만 도달하므로,
		// 여기서 직접 ASC->SetEquippedWeaponType()만 부르면 서버 ASC만 바뀌고 소유 클라이언트의
		// 로컬 ASC 사본은 예전 무기 태그에 멈춰 남는다 - 이후 그 클라이언트의 LocalPredicted
		// Fire/Reload 어빌리티가 새 무기의 ActivationRequiredTags(장착 태그)를 로컬에서 통과하지
		// 못해 조용히 막힌다. PlayerState::SyncEquippedWeaponState()는 서버 세팅 + Client RPC
		// push를 함께 하므로(WeaponLoadoutService가 쓰는 것과 동일한 패턴) 이걸 대신 쓴다.
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(ASC->GetOwner()))
			{
				PlayerState->SyncEquippedWeaponState(CombatComponent->GetCurrentWeapon()->GetEquippedStateTag());
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
