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

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Draft", meta = (ShowOnlyInnerProperties))
	FAugmentDraftRules Rules;

	// 한 번에 제시할 카드 수
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Draft", meta = (ClampMin = "1", ClampMax = "8"))
	int32 OfferCount = 3;

	// 개인 선택 제한 시간. 0 이면 라운드 페이즈가 시간을 관리
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Augment|Draft", meta = (ClampMin = "0.0"))
	float SelectionTimeout = 0.f;
};
