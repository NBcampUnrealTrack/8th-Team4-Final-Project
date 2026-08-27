// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayEffectTypes.h"
#include "Augment/AugmentTypes.h"
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

	void Reset()
	{
		EffectHandles.Reset();
		AbilityHandles.Reset();
	}
};

// 해석이 끝난 어빌리티 한 칸
struct FAugmentResolvedAbility
{
	UClass*      AbilityClass = nullptr;
	FGameplayTag InputTag;
};

// 검증, 로드를 마친 증강
struct FAugmentResolvedGrant
{
	const UAugmentDefinition* Definition = nullptr;

	// 증강 수치와 조건부 규칙
	const FAugmentLevelData* LevelData = nullptr;

	TArray<UClass*> EffectClasses;
		
	TArray<FAugmentResolvedAbility> Abilities;

	bool IsValid() const { return Definition != nullptr && LevelData != nullptr; }
};

namespace RogueAugmentAssembler
{
	// 증강 정의 해석, GE/GA 로드
	FPS_API bool ResolveAugment(const UAugmentDefinition& Definition, FAugmentResolvedGrant& OutResolved);

	// 해석된 증강을 ASC에 적용
	FPS_API FAugmentGrantHandles ApplyResolved(UAbilitySystemComponent& ASC, const FAugmentResolvedGrant& Resolved);

	// ASC에서 증강 제거
	FPS_API void RemoveAugment(UAbilitySystemComponent& ASC, FAugmentGrantHandles& Handles);
}
