// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Augment/AugmentTypes.h"

// 등급 추첨 규칙
struct FAugmentDraftRules
{
	// 등급별 가중치
	TMap<EAugmentTier, FAugmentTierDraftWeight> TierWeights;

	// 패배 팀의 희귀 이상 등장 확률에 곱해지는 배수
	float BehindTierMultiplier = 1.5f;

	// 이 라운드는 강제 등급으로 고정. 0 이면 강제 없음
	int32 ForcedTierRound = 11;

	// 강제 등급
	EAugmentTier ForcedTier = EAugmentTier::Legendary;
};

// 증강 드래프트 관련 함수
namespace RogueAugmentDraft
{
	// 이번 라운드에 한 팀이 받을 등급
	FPS_API EAugmentTier ResolveTier(
		const FAugmentDraftRules& Rules,
		int32 RoundNumber,
		int32 Seed,
		bool bIsBehind,
		EAugmentTier MinimumTier = EAugmentTier::Common);
}
