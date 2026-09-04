// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_WeaponAbilityBase.h"
#include "GA_ADS.generated.h"

// 정조준(ADS)을 홀드 입력으로 트리거하는 GameplayAbility. InputTag.Ability.Secondary를 누르고 있는
// 동안만 조준 상태를 유지하고, 떼거나 어빌리티가 취소/종료되면 반드시 조준을 해제한다.
// AWeaponBase::bIsADS(스프레드 0 고정/반동 판정이 이미 참조 중)와 UCombatComponent::bAiming
// (ShooterAnimInstance가 이미 구독 중인 애니메이션 블렌드)을 동시에 켜고 끈다.
UCLASS()
class FPS_API UGA_ADS : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	UGA_ADS();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                                 const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
	                                 FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                              const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                         const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	// 조준을 끊는 태그(State_Movement_InAir, State_Combat_UtilityHeld)가 켜질 때 호출된다.
	// 스나이퍼로 조준 중일 때만 스스로 취소한다 - 다른 무기는 GetWeaponTag() 비교에서 걸러진다.
	void HandleAimCancelTagChanged(const FGameplayTag Tag, int32 NewCount);

	// 활성화 시점에 조준을 걸어둔 무기/CombatComponent. 도중에 무기가 바뀌어도(예: 조준 중 무기 전환
	// 입력) EndAbility가 지금 장착된 무기가 아니라 "이 어빌리티가 실제로 켰던" 대상을 끄도록 캐싱한다.
	UPROPERTY()
	TObjectPtr<AWeaponBase> AimingWeapon;

	UPROPERTY()
	TObjectPtr<UCombatComponent> AimingCombatComponent;

	// HandleAimCancelTagChanged 구독 해제용. ActivateAbility에서 등록하고 EndAbility에서 반드시 해제한다.
	FDelegateHandle InAirTagEventHandle;
	FDelegateHandle UtilityHeldTagEventHandle;
};
