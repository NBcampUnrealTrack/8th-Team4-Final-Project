// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_RangedAttack.generated.h"

class AWeaponBase;
class UAnimMontage;

// 무기 사격을 트리거하는 GameplayAbility.
// AWeaponBase의 ASC/GameplayTag 관련 로직을 대신 처리하는 유일한 클래스다.
// AWeaponBase 자체는 이 클래스의 존재를 모른다.
UCLASS()
class FPS_API UGA_RangedAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_RangedAttack();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                                 const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
	                                 FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                              const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 발사 시 재생할 몽타주. 애니메이션 완료를 기다리지 않고 Fire()로 바로 이어간다(0ms 반응성 유지).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Animation")
	TObjectPtr<UAnimMontage> FireMontage;

	// TODO(연동 필요 - CombatComponent 담당자): CombatComponent에 현재 장착 무기를 반환하는
	// Getter(예: GetEquippedWeapon())가 추가되면 그 API를 호출하도록 이 함수를 교체할 것.
	// (Slack 논의: "Current 무기 Getter" 관련) 지금은 임시로 Avatar에 Attach된 AWeaponBase를 직접 탐색한다.
	AWeaponBase* GetEquippedWeapon() const;
};