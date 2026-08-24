// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_ADS.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Combat/CombatComponent.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

UGA_ADS::UGA_ADS()
{
	ActivationOwnedTags.AddTag(RogueTags::State_Combat_IsAiming);
}

bool UGA_ADS::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                  const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
                                  FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AWeaponBase* Weapon = GetEquippedWeapon();
	return IsValid(Weapon) && Weapon->CanADS();
}

void UGA_ADS::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                               const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AimingWeapon = GetEquippedWeapon();
	if (!AimingWeapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AimingCombatComponent = GetCombatComponent();

	AimingWeapon->SetADS(true);
	if (AimingCombatComponent)
	{
		AimingCombatComponent->SetAiming(true);
	}

	// bTestAlreadyReleased=true: 이 태스크가 붙기 전에 이미 입력이 떼어진 극단적인 경우(매우 짧게
	// 눌렀다 뗀 프레임)에도 조준이 켜진 채로 멈추지 않도록 즉시 해제 콜백이 오게 한다.
	if (UAbilityTask_WaitInputRelease* WaitTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true))
	{
		WaitTask->OnRelease.AddDynamic(this, &UGA_ADS::HandleInputReleased);
		WaitTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
	}
}

void UGA_ADS::HandleInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGA_ADS::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                          const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	// 정상 종료/취소/다른 어빌리티에 의한 교체 등 어떤 경로로 끝나든 조준은 반드시 꺼야 한다.
	if (AimingWeapon)
	{
		AimingWeapon->SetADS(false);
	}
	if (AimingCombatComponent)
	{
		AimingCombatComponent->SetAiming(false);
	}
	AimingWeapon = nullptr;
	AimingCombatComponent = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}