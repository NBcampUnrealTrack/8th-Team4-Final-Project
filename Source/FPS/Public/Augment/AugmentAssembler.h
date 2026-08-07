// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayEffectTypes.h"
#include "AugmentAssembler.generated.h"

class UAbilitySystemComponent;
class UAugmentDefinition;


// 증강 부여 시 부여된 GE/GA 핸들을 모아둔 구조체
USTRUCT()
struct FAugmentGrantHandles
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;

	bool IsEmpty() const { return EffectHandles.IsEmpty() && AbilityHandles.IsEmpty(); }

	void Reset()
	{
		EffectHandles.Reset();
		AbilityHandles.Reset();
	}
};

// PDA 기반의 증강 데이터를 UAbilitySystemComponent 에 적용하고, 해제할 수 있는 기능 제공
namespace RogueAugmentAssembler
{
	// 서버 전용. 계열의 GE/GA 를 Level 의 수치로 부여하고 해제용 핸들을 반환
	// 정의에 없는 레벨이면 빈 핸들
	FPS_API FAugmentGrantHandles ApplyAugment(UAbilitySystemComponent& ASC,
	                                          const UAugmentDefinition& Definition,
	                                          int32 Level);

	// ApplyAugment 가 돌려준 핸들을 되돌린다. 성공하면 Handles은 비움
	FPS_API void RemoveAugment(UAbilitySystemComponent& ASC, FAugmentGrantHandles& Handles);
}
