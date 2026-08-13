// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"   // TSubclassOf 는 StaticClass() 가 필요해 전방 선언으로 부족하다
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "RogueAbilitySet.generated.h"

// 부여할 어빌리티
USTRUCT(BlueprintType)
struct FRogueGrantedAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	// 이 어빌리티를 발동시키는 입력 슬롯
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Ability",
		meta = (Categories = "InputTag.Ability"))
	FGameplayTag InputTag;
};

UCLASS(BlueprintType)
class FPS_API URogueAbilitySet : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Ability")
	TArray<FRogueGrantedAbility> Abilities;
};
