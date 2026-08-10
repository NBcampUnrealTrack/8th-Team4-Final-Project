// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_WeaponAbilityBase.generated.h"

class AWeaponBase;

// GA_RangedAttack / GA_Reload / GA_MeleeAttack이 공통으로 쓰는 "현재 장착 무기 찾기" 로직을
// 한 곳에 모아둔 베이스 클래스. AWeaponBase의 ASC/GameplayTag 관련 로직을 대신 처리하는
// 어빌리티 계열은 전부 이걸 상속한다. AWeaponBase 자체는 이 클래스들의 존재를 모른다.
UCLASS(Abstract)
class FPS_API UGA_WeaponAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WeaponAbilityBase();

protected:
	// TODO(연동 필요 - CombatComponent 담당자): CombatComponent에 현재 장착 무기를 반환하는
	// Getter(예: GetEquippedWeapon())가 추가되면 그 API를 호출하도록 이 함수 하나만 교체하면 된다.
	// (Slack 논의: "Current 무기 Getter" 관련) 지금은 임시로 Avatar에 Attach된 AWeaponBase를 직접 탐색한다.
	AWeaponBase* GetEquippedWeapon() const;

	// 태그 기반으로 수정
	void SendWeaponGameplayEvent(FGameplayTag EventTag) const;
};