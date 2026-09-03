// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Augment/AugmentTypes.h"
#include "AugmentDefinition.generated.h"

class UGameplayEffect;
class UTexture2D;

// 증강 계열 하나. 애셋 하나가 계열 하나
UCLASS(BlueprintType)
class FPS_API UAugmentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// 증강 수치. 비어 있으면 nullptr
	const FAugmentLevelData* GetLevelData() const
	{
		return Levels.IsValidIndex(0) ? &Levels[0] : nullptr;
	}

	// 액티브 증강이 쓰는 입력 슬롯. 애셋에서 지정하며 액티브가 아니면 무효 태그
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	FGameplayTag GetActiveInputSlotTag() const;

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

	// 계열 이름
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Availability",
		meta = (Categories = "Unlock"))
	FGameplayTagContainer RequiredUnlockTags;

	// --------------------------------------------------
	// 손에 드는 액티브 증강의 비주얼. 액티브가 아니면 무효

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Held")
	FAugmentHeldVisual HeldVisual;

	// --------------------------------------------------
	// 효과. 수치는 Levels 의 0번 칸

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
	TArray<TSoftClassPtr<UGameplayEffect>> GrantedEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
	TArray<FAugmentGrantedAbility> GrantedAbilities;

	// --------------------------------------------------
	// 증강 수치. 0번 칸만 사용한다

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Value",
		meta = (TitleProperty = "Description"))
	TArray<FAugmentLevelData> Levels;
};
