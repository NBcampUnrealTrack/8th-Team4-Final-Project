// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_RangedAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/RPMUtils.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "Weapon/WeaponBase.h"

UGA_RangedAttack::UGA_RangedAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

AWeaponBase* UGA_RangedAttack::GetEquippedWeapon() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return nullptr;
	}

	TArray<AActor*> AttachedActors;
	AvatarActor->GetAttachedActors(AttachedActors);

	for (AActor* AttachedActor : AttachedActors)
	{
		if (AWeaponBase* Weapon = Cast<AWeaponBase>(AttachedActor))
		{
			return Weapon;
		}
	}

	return nullptr;
}

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
		// TODO(GAS 팀): feat-BaseAugment 브랜치가 main에 정식 머지되면 RogueTags::Event_Weapon_Fired로 교체할 것.
		// 지금은 그 브랜치가 아직 들어와 있지 않아 동일한 문자열("Event.Weapon.Fired")을 동적으로 요청해서 쓴다.
		// (참고: RequestGameplayTag는 해당 문자열이 프로젝트 어딘가에 네이티브로 등록돼 있어야 유효한 태그가 되므로,
		//  머지 전까지는 이 이벤트가 실질적으로 무동작일 수 있다.)
		const FGameplayTag WeaponFiredEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Weapon.Fired")));

		FGameplayEventData Payload;
		Payload.EventTag = WeaponFiredEventTag;
		Payload.Instigator = GetAvatarActorFromActorInfo();

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActorFromActorInfo(), WeaponFiredEventTag, Payload);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}