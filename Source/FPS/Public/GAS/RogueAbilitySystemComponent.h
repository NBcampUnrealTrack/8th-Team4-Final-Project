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

	// 눌림 상태 활성화
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	// 떼짐 상태 활성화
	UFUNCTION(BlueprintCallable, Category = "Rogue|Ability")
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	// 목록의 어빌리티를 부여 서버 전용.
	void GrantAbilitySet(const URogueAbilitySet* AbilitySet);
};
