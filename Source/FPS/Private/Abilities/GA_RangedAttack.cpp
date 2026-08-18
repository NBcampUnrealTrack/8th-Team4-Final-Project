// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_RangedAttack.h"

#include "AbilitySystemComponent.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/PlayerController.h"
#include "Player/NetworkTimeSyncComponent.h"
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

	const AWeaponBase* Weapon = GetEquippedWeapon();
	if (!Weapon)
	{
		return false;
	}

	// 연사 입력은 Enhanced Input의 Triggered 동안 매 프레임 활성화를 재시도한다.
	// CanFireNow()를 여기서도 게이트해 실제 발사 간격이 되기 전의 불필요한 LocalPredicted
	// 어빌리티 활성화/RPC를 줄이고, Fire()의 동일 검증은 최종 권위 안전망으로 그대로 둔다.
	return Weapon->CanFireNow();
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

	// ASC 가 없으면 BaseRPM 을 그대로 반환
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		const float FinalRPM = URogueAugmentStatResolver::GetFinalRPM(Avatar, Weapon->GetBaseRPM());
		Weapon->SetEffectiveRPM(FinalRPM);
	}

	float ClientFireServerTime = 0.0f;
	if (ActorInfo)
	{
		if (const APlayerController* PlayerController = ActorInfo->PlayerController.Get())
		{
			if (const UNetworkTimeSyncComponent* TimeSyncComponent = PlayerController->FindComponentByClass<UNetworkTimeSyncComponent>())
			{
				ClientFireServerTime = TimeSyncComponent->GetEstimatedServerTime();
			}
		}
	}

	const bool bFired = Weapon->FireRanged(ClientFireServerTime);

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
