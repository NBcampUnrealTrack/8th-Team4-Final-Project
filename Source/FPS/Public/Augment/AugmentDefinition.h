// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Augment/AugmentTypes.h"
#include "AugmentDefinition.generated.h"

class UGameplayEffect;
class UTexture2D;

// 증강 계열 하나. 레벨은 Levels 배열의 인덱스로 표현한다
UCLASS(BlueprintType)
class FPS_API UAugmentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// --------------------------------------------------
	// 레벨 번호는 1부터. 인덱스와 한 칸 어긋나므로 변환은 여기서만

	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	int32 GetMaxLevel() const { return Levels.Num(); }

	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	bool HasLevel(int32 Level) const { return Levels.IsValidIndex(Level - 1); }

	// 없는 레벨이면 nullptr
	const FAugmentLevelData* FindLevel(int32 Level) const
	{
		return Levels.IsValidIndex(Level - 1) ? &Levels[Level - 1] : nullptr;
	}

	// AssetManager 에 등록하는 타입 이름.
	static const FPrimaryAssetType AugmentAssetType;

	// --------------------------------------------------
	// 식별

	// 증강 계열. 애셋 하나가 계열 하나다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Identity",
		meta = (Categories = "Augment.Line"))
	FGameplayTag LineTag;

	// --------------------------------------------------
	// 표시 (레벨 공통)

	// 계열 이름. 레벨 표기(I / II / III)는 UI에서 추가
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Display",
		meta = (AssetBundles = "UI"))
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Display")
	EAugmentCategory AugmentCategory = EAugmentCategory::Passive;

	// 티어는 각 증강의 레벨 공통
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Display")
	EAugmentTier Tier = EAugmentTier::Common;

	// --------------------------------------------------
	// 출현 조건

	// 전부 보유해야 상점에 등장 (Unlock.Weapon.* 등). 비어 있으면 조건 없음
	// 레벨 선행 조건은 포함 X, 보유 레벨 + 1 로 이미 표현
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Availability",
		meta = (Categories = "Unlock"))
	FGameplayTagContainer RequiredUnlockTags;

	// --------------------------------------------------
	// 효과 (레벨 공통). 레벨마다 달라지는 것은 Levels 의 수치

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
	TArray<TSoftClassPtr<UGameplayEffect>> GrantedEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
	TArray<FAugmentGrantedAbility> GrantedAbilities;

	// --------------------------------------------------
	// 레벨. 0번 칸이 1레벨. 밸런싱은 이 배열을 위아래로 훑으며 한다

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Level",
		meta = (TitleProperty = "Description"))
	TArray<FAugmentLevelData> Levels;
};
