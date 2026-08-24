// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_MeleeAttack.h"

#include "AbilitySystemComponent.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "Animation/AnimInstance.h"
#include "Tags/GameplayTags.h"
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

	// ASC 가 없으면 BaseRPM 을 그대로 반환
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		// 좌/우클릭은 서로 다른 RPM(쿨다운)을 쓰므로, 지금 발동한 쪽의 RPM만 갱신한다.
		if (bIsHeavyAttack && Weapon->GetAltBaseRPM() > 0.f)
		{
			const float FinalAltRPM = URogueAugmentStatResolver::GetFinalRPM(Avatar, Weapon->GetAltBaseRPM());
			Weapon->SetEffectiveAltRPM(FinalAltRPM);
		}
		else
		{
			const float FinalRPM = URogueAugmentStatResolver::GetFinalRPM(Avatar, Weapon->GetBaseRPM());
			Weapon->SetEffectiveRPM(FinalRPM);
		}

		const int32 FinalPelletCount = URogueAugmentStatResolver::GetFinalPelletCount(Avatar, Weapon->GetPelletCountPerShot());
		Weapon->SetEffectivePelletCount(FinalPelletCount);

		const float FinalSpreadDegrees = URogueAugmentStatResolver::GetFinalSpread(Avatar, Weapon->GetBaseSpreadDegrees());
		Weapon->SetEffectiveSpreadDegrees(FinalSpreadDegrees);

		// TraceRange(cm)를 m로 변환해 GetFinalMeleeRange에 넘기고, 결과를 다시 cm로 되돌려 배선한다.
		// 원거리 무기의 사거리/관통은 데미지 파이프라인이 별도로 처리하므로 여기서는 근접무기만 다룬다.
		const float FinalMeleeRangeMeters = URogueAugmentStatResolver::GetFinalMeleeRange(Avatar, Weapon->GetTraceRange() / 100.f);
		Weapon->SetEffectiveTraceRange(FinalMeleeRangeMeters * 100.f);
	}

	if (AttackMontage && ActorInfo)
	{
		if (UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance())
		{
			AnimInstance->Montage_Play(AttackMontage);
		}
	}

	const bool bAttacked = Weapon->FireMelee(bIsHeavyAttack);

	if (bAttacked)
	{
		SendWeaponGameplayEvent(RogueTags::Event_Weapon_Fired);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
