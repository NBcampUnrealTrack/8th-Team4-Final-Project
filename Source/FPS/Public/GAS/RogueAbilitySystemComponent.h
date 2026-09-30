// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"

#include "RogueAbilitySystemComponent.generated.h"

class URogueAbilitySet;

UCLASS()
class FPS_API URogueAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	URogueAbilitySystemComponent();
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	// 눌림 상태 활성화
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	// 떼짐 상태 활성화
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	// 목록의 어빌리티를 부여 서버 전용.
	void GrantAbilitySet(const URogueAbilitySet* AbilitySet);

	// 현재 장착 무기의 장착 상태 태그(State.Weapon.Equipped.*)를 붙임. 이전 태그는 제거.
	// 무기 종류 판별은 호출부(WeaponBase/CombatComponent) 책임이며, 이 ASC는 태그만 받는다.
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void SetEquippedWeaponType(FGameplayTag EquippedStateTag);

	// 수명이 라운드에 한정된 GE 제거
	int32 RemoveRoundScopedEffects();

private:
	// GAS clears Spec.InputPressed on EndAbility; keep physical holds separately.
	TSet<FGameplayTag> HeldMeleeInputs;
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
