// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Weapon/WeaponTypes.h"   // EWeaponType 은 UFUNCTION 인자라 전방 선언으로 부족하다

#include "RogueAbilitySystemComponent.generated.h"

class URogueAbilitySet;

UCLASS()
class FPS_API URogueAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	URogueAbilitySystemComponent();

	// 눌림 상태 활성화
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	// 떼짐 상태 활성화
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	// 목록의 어빌리티를 부여 서버 전용.
	void GrantAbilitySet(const URogueAbilitySet* AbilitySet);

	// 현재 장착 무기 종류를 장착 상태 태그로 바꿔서 붙임
	// 이전 태그는 제거
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void SetEquippedWeaponType(EWeaponType WeaponType);

private:
	// 지금 붙여둔 장착 태그. 교체 시 정확히 이것만 걷어내려고 들고 있는다
	FGameplayTag CurrentEquippedWeaponTag;
};
