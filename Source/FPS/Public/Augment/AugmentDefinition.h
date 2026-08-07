// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Augment/AugmentTypes.h"
#include "AugmentDefinition.generated.h"

class URogueGameplayAbility;
class UTexture2D;

UCLASS(BlueprintType)
class FPS_API UAugmentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// 하위 레벨로부터 상위 레벨로 교체되는 증강인지. Level 1 은 기본, Level 2 이상은 업그레이드
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	bool IsUpgrade() const { return Level > 1; }

public:
	// AssetManager 에 등록하는 타입 이름.
	static const FPrimaryAssetType AugmentAssetType;

	// --------------------------------------------------
	// 식별

	// 증강 계열. 같은 계열은 레벨업
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Identity",
		meta = (Categories = "Augment.Line"))
	FGameplayTag LineTag;

	// 계열 내 레벨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Identity",
		meta = (ClampMin = "1"))
	int32 Level = 1;

	// --------------------------------------------------
	// 표시

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Display", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Display",
		meta = (AssetBundles = "UI"))
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Display")
	EAugmentTier Tier = EAugmentTier::Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Display")
	EAugmentCategory AugmentCategory = EAugmentCategory::Stat;

	// --------------------------------------------------
	// 출현 조건

	// 전부 보유해야 상점에 등장 (Unlock.Weapon.* 등). 비어 있으면 조건 없음
	// 레벨 선행 조건은 포함 X, LineTag + Level 로 이미 표현
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Availability",
		meta = (Categories = "Unlock"))
	FGameplayTagContainer RequiredUnlockTags;

	// --------------------------------------------------
	// 효과
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
	TArray<FAugmentGrantedEffect> GrantedEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
	TArray<FAugmentGrantedAbility> GrantedAbilities;

	// ExecCalc 이 히트마다 컨텍스트와 대조하는 규칙.
	// 어트리뷰트 축으로 표현할 수 없는 조건부 대미지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
	TArray<FAugmentConditionalDamageRule> ConditionalDamageRules;
};
