// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_RangedAttack.h"

#include "AbilitySystemComponent.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "Animation/AnimInstance.h"
#include "Animation/ShooterAnimInstance.h"
#include "GAS/RogueAbilitySystemComponent.h"
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

	if (Weapon->IsOutOfAmmo() && !Weapon->IsReloading())
	{
		// 탄창이 빈 뒤 첫 사격 입력엔 클릭음만 재생한다(TryPlayEmptyClipSound가 true 반환).
		// 이미 한 번 클릭음을 들은 뒤 또 사격 입력이 들어오면(false 반환), 재장전 입력을 누른 것과
		// 똑같이 처리해 자동으로 재장전을 시작한다 - 매번 R을 따로 눌러야 하는 불편을 없앤다.
		if (!Weapon->TryPlayEmptyClipSound())
		{
			if (URogueAbilitySystemComponent* RogueASC = Cast<URogueAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
			{
				static const FGameplayTag ReloadInputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Reload"));
				RogueASC->AbilityInputTagPressed(ReloadInputTag);
			}
		}
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

		// 반동은 지향 사격/ADS 공용으로 적용된다(WeaponBase::TickProceduralRecoilGeneration). 매번 최신 배율을 반영해둔다.
		// GetFinalRecoil은 배율(1.0 기준) 함수라 Base 인자로 1.0을 넘긴다.
		const float FinalRecoilMultiplier = URogueAugmentStatResolver::GetFinalRecoil(Avatar, 1.f);
		Weapon->SetEffectiveRecoilMultiplier(FinalRecoilMultiplier);
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
		// 몽타주 자체는 무기 태그별로 WeaponData에서 찾아 재생한다(ShooterAnimInstance::PlayFireMontageForCurrentWeapon).
		if (ActorInfo)
		{
			if (UShooterAnimInstance* ShooterAnim = Cast<UShooterAnimInstance>(ActorInfo->GetAnimInstance()))
			{
				ShooterAnim->PlayFireMontageForCurrentWeapon();
			}
		}

		SendWeaponGameplayEvent(RogueTags::Event_Weapon_Fired);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
