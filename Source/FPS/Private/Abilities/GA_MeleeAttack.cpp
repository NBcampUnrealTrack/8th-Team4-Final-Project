// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_MeleeAttack.h"

#include "AbilitySystemComponent.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "Animation/AnimInstance.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/PlayerInterface.h"

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
	if (Weapon->IsReloading())
	{
		return false;
	}

	// Primary 입력은 AR 연사용으로 Triggered(누르고 있는 동안 매 프레임) 바인딩이라 칼도 매 프레임
	// 활성화를 재시도한다. 쿨다운 중에도 활성화되면 몽타주가 매 프레임 처음부터 다시 재생돼
	// 버튼을 뗄 때서야 휘두르는 것처럼 보이므로, GA_RangedAttack과 같이 CanFireNow()로 게이트한다.
	// 이렇게 하면 꾹 누르고 있는 동안 RPM 간격마다 연속으로 휘두른다.
	return Weapon->CanFireNow(bIsHeavyAttack);
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

	const bool bAttacked = Weapon->FireMelee(bIsHeavyAttack);

	if (bAttacked)
	{
		// 실제 공격이 승인된 경우에만 몽타주를 재생해, 쿨다운 중 재활성화로 몽타주가 리셋되지 않게 한다.
		if (AttackMontage && ActorInfo)
		{
			UAnimInstance* BodyAnimInstance = ActorInfo->GetAnimInstance();
			if (BodyAnimInstance)
			{
				BodyAnimInstance->Montage_Play(AttackMontage);
			}

			// GAS caches CharacterMesh0, not the separate owner-visible first-person arms.
			// Preserve the existing body path and explicitly play on Mesh1P as well.
			AActor* Avatar = GetAvatarActorFromActorInfo();
			if (IsValid(Avatar) && Avatar->Implements<UPlayerInterface>())
			{
				USkeletalMeshComponent* Arms = IPlayerInterface::Execute_GetMesh1P(Avatar);
				UAnimInstance* ArmsAnimInstance = IsValid(Arms) ? Arms->GetAnimInstance() : nullptr;
				if (ArmsAnimInstance && ArmsAnimInstance != BodyAnimInstance)
				{
					ArmsAnimInstance->Montage_Play(AttackMontage);
				}
			}
		}

		SendWeaponGameplayEvent(RogueTags::Event_Weapon_Fired);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
