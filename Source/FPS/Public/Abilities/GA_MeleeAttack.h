// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_WeaponAbilityBase.h"
#include "GA_MeleeAttack.generated.h"

class UAnimMontage;

// 전술 칼 등 근접무기 공격을 트리거하는 GameplayAbility.
// bIsHeavyAttack으로 좌클릭(약공격, BaseDamage)/우클릭(강공격, AltBaseDamage)을 구분한다.
// 예: 이 클래스를 상속한 GA_MeleeAttack_Light(bIsHeavyAttack=false)와
// GA_MeleeAttack_Heavy(bIsHeavyAttack=true)를 블루프린트로 만들어 각각 다른 입력에 연결한다.
UCLASS()
class FPS_API UGA_MeleeAttack : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                                 const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
	                                 FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                              const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// false = 좌클릭 약공격 (AWeaponBase::BaseDamage), true = 우클릭 강공격 (AWeaponBase::AltBaseDamage)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Melee")
	bool bIsHeavyAttack = false;

	// 공격 시 재생할 몽타주. 애니메이션 완료를 기다리지 않고 Fire()로 바로 이어간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;
};