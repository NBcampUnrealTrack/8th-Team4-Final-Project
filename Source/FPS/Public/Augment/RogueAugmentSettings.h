// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Augment/AugmentTypes.h"
#include "Engine/DeveloperSettings.h"

#include "RogueAugmentSettings.generated.h"

// 증강 전용 설정. GAS 공용 설정과 분리
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Rogue Augment"))
class FPS_API URogueAugmentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URogueAugmentSettings();

	// 라운드 증강의 등급 추첨 가중치. 기본값은 생성자에 있고 ini 로 덮어쓸 수 있음
	UPROPERTY(Config, EditAnywhere, Category = "Draft")
	TMap<EAugmentTier, FAugmentTierDraftWeight> TierDraftWeights;
};
