// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Augment/AugmentTypes.h"

class UAugmentDefinition;

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

	// 보유 증강 상한
	int32 MaxOwnedLines = 5;

	// 보유 액티브 증강 상한
	int32 MaxActiveLines = 2;
};

// 후보 판정에 쓰는 플레이어 상태
struct FAugmentDraftEligibility
{
	// 이번 라운드 등급
	EAugmentTier Tier = EAugmentTier::Common;

	// 보유 목록. 인벤토리의 Entries
	TArray<FAugmentInventoryEntry> Owned;

	// 보유 중인 액티브 증강 수
	int32 OwnedActiveCount = 0;

	// ASC 가 보유한 해금 태그
	FGameplayTagContainer UnlockTags;

	// 매치 전체 등장 이력, 현재는 동작 X
	TSet<FPrimaryAssetId> SeenDefinitions;
};

// 증강 추첨 결과
struct FAugmentOfferResult
{
	// 플레이어에게 제시할 증강 후보
	TArray<FAugmentInventoryEntry> Offers;

	// 증강이 부족 해 못채운 슬롯 수
	int32 UnfilledSlots = 0;

	// 0번 칸 레벨업 증강 여부
	bool bLevelUpSlotReserved = false;
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

	// 후보가 될 증강의 레벨을 판정
	FPS_API int32 GetOfferLevel(
		const FAugmentDraftEligibility& State,
		const UAugmentDefinition& Definition);

	// 해당 증강이 이번 라운드 후보로 제시될 수 있는지 판정
	FPS_API bool IsCandidateEligible(
		const FAugmentDraftRules& Rules,
		const FAugmentDraftEligibility& State,
		const UAugmentDefinition& Definition);

	// 후보로 제시될 수 있는 증강 모은 버킷
	FPS_API void BuildCandidateBucket(
		const FAugmentDraftRules& Rules,
		const FAugmentDraftEligibility& State,
		const TArray<UAugmentDefinition*>& Definitions,
		TArray<FAugmentInventoryEntry>& OutBucket);

	// 버킷에서 플레이어에게 제시할 증강 뽑기
	FPS_API FAugmentOfferResult PickOffers(
		TArray<FAugmentInventoryEntry>& Bucket,
		int32 CardCount,
		int32 Seed);
}
