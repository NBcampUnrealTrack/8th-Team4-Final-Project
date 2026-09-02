// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_WeaponAbilityBase.h"
#include "Weapon/WeaponTypes.h"
#include "GA_SwitchWeapon.generated.h"

// 무기 슬롯 전환을 트리거하는 GameplayAbility. GA_RangedAttack/GA_Reload와 같은 패턴으로,
// 실제 스왑 상태 머신(스폰/부착/장착 타이머)은 UCombatComponent::SwitchWeaponSlot()이 갖고 있고,
// 이 어빌리티는 검증 이후 그 함수를 호출하고 성공 시 Event.Weapon.Swap만 발신한다.
// Primary/Secondary 슬롯마다 이 클래스를 상속한 BP를 하나씩 만들어 TargetWeaponSlot과
// 서로 다른 InputTag를 채워 AbilitySet에 등록한다.
UCLASS()
class FPS_API UGA_SwitchWeapon : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	UGA_SwitchWeapon();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                                 const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
	                                 FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                              const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 스왑 애니메이션 도중에도 다른 슬롯으로 다시 스왑할 수 있어야 하므로 이 게이트를 건너뛴다.
	virtual bool RequiresWeaponReady() const override { return false; }


	// 이 어빌리티가 전환할 목표 슬롯.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Loadout")
	EWeaponSlot TargetWeaponSlot = EWeaponSlot::Primary;
};
