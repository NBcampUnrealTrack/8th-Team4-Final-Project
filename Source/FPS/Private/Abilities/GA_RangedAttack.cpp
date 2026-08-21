// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_RangedAttack.h"

#include "AbilitySystemComponent.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "Animation/AnimInstance.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

bool UGA_RangedAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                           const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
                                           FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	AWeaponBase* Weapon = GetEquippedWeapon();
	if (!Weapon)
	{
		return false;
	}

	// 연사 입력은 Enhanced Input의 Triggered 동안 매 프레임 활성화를 재시도한다.
	// CanFireNow()를 여기서도 게이트해 실제 발사 간격이 되기 전의 불필요한 LocalPredicted
	// 어빌리티 활성화/RPC를 줄이고, Fire()의 동일 검증은 최종 권위 안전망으로 그대로 둔다.
	if (Weapon->CanFireNow())
	{
		return true;
	}

	// 탄약이 없어서 못 쐈다면 빈 탄창 사운드를 시도한다(내부적으로 RPM 간격만큼 스스로 디바운스함).
	if (Weapon->IsOutOfAmmo())
	{
		Weapon->TryPlayEmptyClipSound();
	}

	return false;
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

	// ASC 가 없으면 Base 값을 그대로 반환
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		const float FinalRPM = URogueAugmentStatResolver::GetFinalRPM(Avatar, Weapon->GetBaseRPM());
		Weapon->SetEffectiveRPM(FinalRPM);

		const int32 FinalPelletCount = URogueAugmentStatResolver::GetFinalPelletCount(Avatar, Weapon->GetPelletCountPerShot());
		Weapon->SetEffectivePelletCount(FinalPelletCount);

		const float FinalSpreadDegrees = URogueAugmentStatResolver::GetFinalSpread(Avatar, Weapon->GetBaseSpreadDegrees());
		Weapon->SetEffectiveSpreadDegrees(FinalSpreadDegrees);

		// 반동은 ADS 중에만 실제로 적용되지만(WeaponBase::TickProceduralRecoilGeneration), 매번 최신 배율을 반영해둔다.
		// GetFinalRecoil은 배율(1.0 기준) 함수라 Base 인자로 1.0을 넘긴다.
		const float FinalRecoilMultiplier = URogueAugmentStatResolver::GetFinalRecoil(Avatar, 1.f);
		Weapon->SetEffectiveRecoilMultiplier(FinalRecoilMultiplier);
	}

	const bool bFired = Weapon->Fire();

	if (bFired)
	{
		// 연사 입력은 Triggered 동안 반복 활성화되므로 실제 발사가 승인된 프레임에만 몽타주를 재생한다.
		if (FireMontage && ActorInfo)
		{
			if (UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance())
			{
				AnimInstance->Montage_Play(FireMontage);
			}
		}

		SendWeaponGameplayEvent(RogueTags::Event_Weapon_Fired);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}