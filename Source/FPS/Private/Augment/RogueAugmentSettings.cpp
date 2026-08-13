// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/RogueAugmentSettings.h"

URogueAugmentSettings::URogueAugmentSettings()
{
	// 기획 확률표 그대로. 1R 65 / 30 / 5 / 0 에서 라운드마다 -3 / +1.8 / +1 / +0.2.
	// 증감 합이 0 이라 총합 100 이 라운드가 지나도 유지
	// 여기 값을 고쳐도 DefaultGame.ini 에 이미 값이 있으면 오버라이드
	TierDraftWeights.Add(EAugmentTier::Common,    { 65.f, -3.0f });
	TierDraftWeights.Add(EAugmentTier::Rare,      { 30.f,  1.8f });
	TierDraftWeights.Add(EAugmentTier::Epic,      {  5.f,  1.0f });
	TierDraftWeights.Add(EAugmentTier::Legendary, {  0.f,  0.2f });
}
