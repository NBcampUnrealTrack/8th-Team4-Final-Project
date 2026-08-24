// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_WeaponAbilityBase.generated.h"

class AWeaponBase;
class UCombatComponent;

// GA_RangedAttack / GA_Reload / GA_MeleeAttack이 공통으로 쓰는 "현재 장착 무기 찾기" 로직을
// 한 곳에 모아둔 베이스 클래스. AWeaponBase의 ASC/GameplayTag 관련 로직을 대신 처리하는
// 어빌리티 계열은 전부 이걸 상속한다. AWeaponBase 자체는 이 클래스들의 존재를 모른다.
UCLASS(Abstract)
class FPS_API UGA_WeaponAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WeaponAbilityBase();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                                const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
	                                FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	// Avatar에 붙은 UCombatComponent를 찾아준다. 무기 탐색/스왑/부착 로직은 전부 CombatComponent가
	// 전담하므로, 파생 어빌리티는 이 컴포넌트를 통해서만 무기 상태에 접근한다.
	UCombatComponent* GetCombatComponent() const;

	// Avatar의 UCombatComponent::GetEquippedWeapon()을 그대로 위임한다 (무기 탐색/부착 로직은
	// CombatComponent가 전담 - 이 클래스는 GAS 쪽에서 쓰기 편하게 짧은 이름으로 감싸주는 것뿐).
	AWeaponBase* GetEquippedWeapon() const;

	// 태그 기반으로 수정
	void SendWeaponGameplayEvent(FGameplayTag EventTag) const;
};
