// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Augment/AugmentDraft.h"

#include "AugmentDraftProfile.generated.h"

// 게임모드 하나가 쓰는 증강 추첨 규칙 묶음
UCLASS(BlueprintType)
class FPS_API UAugmentDraftProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	const FAugmentDraftRules& GetRules() const { return Rules; }
	int32 GetOfferCount() const { return OfferCount; }
	float GetSelectionTimeout() const { return SelectionTimeout; }
	int32 GetRerollsPerSlot() const { return RerollsPerSlot; }
	bool ShouldIncludeBonusRerolls() const { return bIncludeBonusRerolls; }
	bool RequiresCompleteOfferSet() const { return bRequireCompleteOfferSet; }
	bool ExcludesPreviouslyOfferedDefinitions() const { return bExcludePreviouslyOfferedDefinitions; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Draft", meta = (ShowOnlyInnerProperties))
	FAugmentDraftRules Rules;

	// 한 번에 제시할 카드 수
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Draft", meta = (ClampMin = "1", ClampMax = "8"))
	int32 OfferCount = 3;

	// 개인 선택 제한 시간. 0 이면 라운드 페이즈가 시간을 관리
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Draft", meta = (ClampMin = "0.0"))
	float SelectionTimeout = 0.f;

	// 카드 한 칸마다 허용할 무료 리롤 횟수
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Draft", meta = (ClampMin = "0"))
	int32 RerollsPerSlot = 1;

	// 플레이어의 추가 무료 리롤 스탯을 반영할지 여부
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Draft")
	bool bIncludeBonusRerolls = true;

	// 모든 카드와 리롤 예비를 채울 수 있을 때만 선택 세션을 연다.
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Draft")
	bool bRequireCompleteOfferSet = false;

	// 이전 선택 세션에 등장했던 카드를 매치가 끝날 때까지 다시 제외할지 여부
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Draft")
	bool bExcludePreviouslyOfferedDefinitions = true;
};
