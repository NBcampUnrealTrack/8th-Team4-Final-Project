// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_ADS.h"

#include "AbilitySystemComponent.h"
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
	// 재장전 중에는 정조준을 걸 수 없다 - 재장전 애니메이션 중 정조준 자세로 전환되는 버그 방지.
	return IsValid(Weapon) && Weapon->CanADS() && !Weapon->IsReloading();
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

	// 스나이퍼로 조준 중 공중에 뜨면(점프/낙하) 즉시 줌을 풀어야 한다 - 이동 컴포넌트를 직접 보지
	// 않고 State_Movement_InAir 태그 변화만 구독한다(GAS 설계 원칙 - 어빌리티는 태그로만 판단).
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		InAirTagEventHandle = AbilitySystem->RegisterGameplayTagEvent(
			RogueTags::State_Movement_InAir, EGameplayTagEventType::NewOrRemoved).AddUObject(
				this, &UGA_ADS::HandleAimCancelTagChanged);

		UtilityHeldTagEventHandle = AbilitySystem->RegisterGameplayTagEvent(
			RogueTags::State_Combat_UtilityHeld, EGameplayTagEventType::NewOrRemoved).AddUObject(
				this, &UGA_ADS::HandleAimCancelTagChanged);
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

void UGA_ADS::HandleAimCancelTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	// 공중 진입(NewCount > 0)이고, 현재 이 어빌리티가 켜둔 무기가 저격소총일 때만 스스로 취소한다.
	// 권총/AR 등은 GetWeaponTag() 비교에서 걸러져 공중에서도 계속 조준을 유지한다.
	if (NewCount <= 0 || !AimingWeapon || AimingWeapon->GetWeaponTag() != RogueTags::Weapon_Type_Sniper)
	{
		return;
	}

	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UGA_ADS::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                          const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	// ActivateAbility에서 등록한 InAir 태그 구독을 반드시 해제한다 - 모든 종료 경로(정상 종료/취소/
	// 무기 교체 등)가 EndAbility를 거치므로 여기 한 곳에서만 정리하면 충분하다.
	if (InAirTagEventHandle.IsValid())
	{
		if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
		{
			AbilitySystem->RegisterGameplayTagEvent(
				RogueTags::State_Movement_InAir, EGameplayTagEventType::NewOrRemoved).Remove(InAirTagEventHandle);
		}
		InAirTagEventHandle.Reset();
	}

	if (UtilityHeldTagEventHandle.IsValid())
	{
		if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
		{
			AbilitySystem->RegisterGameplayTagEvent(
				RogueTags::State_Combat_UtilityHeld, EGameplayTagEventType::NewOrRemoved).Remove(UtilityHeldTagEventHandle);
		}
		UtilityHeldTagEventHandle.Reset();
	}

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
