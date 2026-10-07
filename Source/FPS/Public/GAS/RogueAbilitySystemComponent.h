// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"

#include "RogueAbilitySystemComponent.generated.h"

class URogueAbilitySet;
class AWeaponBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCooldownTimeChanged, FGameplayTag /*CooldownTag*/);

UCLASS()
class FPS_API URogueAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	URogueAbilitySystemComponent();

	// 눌림 상태 활성화
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void AbilityInputTagPressed(const FGameplayTag& InputTag, bool bToggleADS = false, bool bIsNewPress = true);

	// 떼짐 상태 활성화
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void AbilityInputTagReleased(const FGameplayTag& InputTag, bool bPreserveADS = false);

	// 실제 무기 전환은 이전 무기의 ADS와 그 입력만 정리한다.
	void CancelADSAbilityForWeapon(const AWeaponBase* Weapon);
	void ClearAbilityInput();
	void ReleaseADSInput();

	// 목록의 어빌리티를 부여 서버 전용.
	void GrantAbilitySet(const URogueAbilitySet* AbilitySet);

	// 현재 장착 무기의 장착 상태 태그(State.Weapon.Equipped.*)를 붙임. 이전 태그는 제거.
	// 무기 종류 판별은 호출부(WeaponBase/CombatComponent) 책임이며, 이 ASC는 태그만 받는다.
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void SetEquippedWeaponType(FGameplayTag EquippedStateTag);

	// 수명이 라운드에 한정된 GE 제거
	int32 RemoveRoundScopedEffects();

	// 남은 액티브 쿨타임을 비율만큼 줄임. 서버 전용
	void ReduceActiveCooldowns(float Rate);

	// 슬롯 쿨타임의 시간이 바뀜 (태그 개수 변화 없이). 서버, 클라 모두
	FOnCooldownTimeChanged OnCooldownTimeChanged;

	virtual void OnGameplayEffectDurationChange(FActiveGameplayEffect& ActiveEffect) override;

private:
	void ReleaseAbilityInput(FGameplayAbilitySpec& Spec);

	// 입력이 들어와도 AbilitySpec이 실행되지 않는 경우 로그
	void ReportInputResult(const FGameplayTag& InputTag, int32 MatchedSpecCount, int32 CandidateCount, int32 ActivatedCount);

	// 지금 붙여둔 장착 태그. 교체 시 정확히 이것만 걷어내려고 들고 있는다
	FGameplayTag CurrentEquippedWeaponTag;

	// 로그를 나누기 위한 시간 기록
	struct FInputDiagnostic
	{
		double LastActivatedTime = 0.0;
		double LastLogTime       = 0.0;
	};
	TMap<FGameplayTag, FInputDiagnostic> InputDiagnostics;
};
