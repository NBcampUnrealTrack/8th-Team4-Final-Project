// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentDraft.h"

#include "Augment/AugmentDefinition.h"

namespace
{
	// 등급 순서 비교, EAugmentTier 선언 순서에 의존
	// Common < Rare < Epic < Legendary
	bool IsLowerTier(const EAugmentTier A, const EAugmentTier B)
	{
		return static_cast<uint8>(A) < static_cast<uint8>(B);
	}

	// TArray<float> 의 총합. 0 이하이면 반드시 Common
	float SumWeights(const TArray<float>& Weights)
	{
		float Total = 0.f;
		for (const float Weight : Weights)
		{
			Total += Weight;
		}

		return Total;
	}

	// bApplyRoundScale 이 false 면 라운드 증감을 빼고 1R 값 그대로
	void BuildWeights(
		const FAugmentDraftRules& Rules,
		const int32 RoundNumber,
		const bool bApplyRoundScale,
		TArray<float>& OutWeights)
	{
		const int32 TierCount = static_cast<int32>(EAugmentTier::MAX);

		OutWeights.Reset();
		OutWeights.SetNumZeroed(TierCount);

		const float Rounds = bApplyRoundScale
			? static_cast<float>(FMath::Max(RoundNumber, 1) - 1)
			: 0.f;

		for (int32 Index = 0; Index < TierCount; ++Index)
		{
			const FAugmentTierDraftWeight* Weight =
				Rules.TierWeights.Find(static_cast<EAugmentTier>(Index));

			if (Weight)
			{
				// 음수 가중치 방지
				OutWeights[Index] = FMath::Max(Weight->BaseWeight + Weight->PerRoundDelta * Rounds, 0.f);
			}
		}
	}

	// 패배 팀 보정, 희귀 이상에 배수를 걸고 일반은 잔여로 채움
	void ApplyBehindBonus(TArray<float>& Weights, const float Multiplier)
	{
		const int32 CommonIndex = static_cast<int32>(EAugmentTier::Common);
		if (!Weights.IsValidIndex(CommonIndex))
		{
			return;
		}

		// 보정 전 총합을 계산
		const float TotalBefore = SumWeights(Weights);

		float UpperTotal = 0.f;
		for (int32 Index = 0; Index < Weights.Num(); ++Index)
		{
			if (Index == CommonIndex)
			{
				continue;
			}

			Weights[Index] *= Multiplier;
			UpperTotal += Weights[Index];
		}

		// Common 등급을 잔여로 채움, 음수는 0으로 클램핑
		Weights[CommonIndex] = FMath::Max(TotalBefore - UpperTotal, 0.f);
	}

	// 가중치에 따라 등급 추첨
	EAugmentTier DrawFromWeights(const TArray<float>& Weights, FRandomStream& Stream)
	{
		// 총합이 0 이면 오류 상황, Common 반환
		const float Total = SumWeights(Weights);
		if (Total <= 0.f)
		{
			return EAugmentTier::Common;
		}

		const float Roll = Stream.FRandRange(0.f, Total);

		// 가중치 누적 합으로 등급 결정
		float Accumulated = 0.f;
		for (int32 Index = 0; Index < Weights.Num(); ++Index)
		{
			Accumulated += Weights[Index];

			if (Weights[Index] > 0.f && Roll <= Accumulated)
			{
				return static_cast<EAugmentTier>(Index);
			}
		}

		// 부동소수 오차로 끝까지 온 경우. 가중치가 있는 마지막 등급
		for (int32 Index = Weights.Num() - 1; Index >= 0; --Index)
		{
			if (Weights[Index] > 0.f)
			{
				return static_cast<EAugmentTier>(Index);
			}
		}

		return EAugmentTier::Common;
	}
}

// 외부에서 호출하는 등급 추첨 함수, 최소 등급 보장
EAugmentTier RogueAugmentDraft::ResolveTier(
	const FAugmentDraftRules& Rules,
	const int32 RoundNumber,
	const int32 Seed,
	const bool bIsBehind,
	const EAugmentTier MinimumTier)
{
	EAugmentTier Result = EAugmentTier::Common;

	// 강제 등급 라운드인 경우 강제 등급 반환
	if (Rules.ForcedTierRound > 0 && RoundNumber == Rules.ForcedTierRound)
	{
		Result = Rules.ForcedTier;
	}
	else
	{
		// 라운드 가중치 계산
		TArray<float> Weights;
		BuildWeights(Rules, RoundNumber, /*bApplyRoundScale*/ true, Weights);

		// 패배 팀 보정 적용
		if (bIsBehind)
		{
			ApplyBehindBonus(Weights, Rules.BehindTierMultiplier);
		}

		// 가중치가 모두 0 이면 1라운드 값으로 재계산
		if (SumWeights(Weights) <= 0.f)
		{
			BuildWeights(Rules, RoundNumber, /*bApplyRoundScale*/ false, Weights);
		}

		// 가중치에 따라 등급 추첨
		FRandomStream Stream(Seed);
		Result = DrawFromWeights(Weights, Stream);
	}

	// 최소 등급 보장
	return IsLowerTier(Result, MinimumTier) ? MinimumTier : Result;
}

// 보유 중이면 다음 레벨, 아니면 1레벨
int32 RogueAugmentDraft::GetOfferLevel(
	const FAugmentDraftEligibility& State,
	const UAugmentDefinition& Definition)
{
	for (const FAugmentInventoryEntry& Entry : State.Owned)
	{
		if (Entry.LineTag.MatchesTagExact(Definition.LineTag))
		{
			return Entry.Level + 1;
		}
	}

	return 1;
}

// 해당 증강이 이번 라운드 후보로 제시될 수 있는지 판정
bool RogueAugmentDraft::IsCandidateEligible(
	const FAugmentDraftRules& Rules,
	const FAugmentDraftEligibility& State,
	const UAugmentDefinition& Definition)
{
	// 라인 태그가 유효하지 않은 경우
	if (!Definition.LineTag.IsValid())
	{
		return false;
	}

	// 등급이 다른 경우
	if (Definition.Tier != State.Tier)
	{
		return false;
	}

	// 다음 레벨이 없는 경우
	const int32 OfferLevel = GetOfferLevel(State, Definition);
	if (!Definition.HasLevel(OfferLevel))
	{
		return false;
	}

	// 이미 보유한 계열이면 레벨업 증강의 후보
	const bool bIsLevelUp = OfferLevel > 1;

	// 필요한 태그가 없는 경우
	if (!State.UnlockTags.HasAll(Definition.RequiredUnlockTags))
	{
		return false;
	}

	// 인벤토리가 가득 찬 경우, 레벨업은 예외
	if (!bIsLevelUp && State.Owned.Num() >= Rules.MaxOwnedLines)
	{
		return false;
	}

	// 액티브 증강이 보유 상한에 걸린 경우, 레벨업은 예외
	if (!bIsLevelUp
		&& Definition.AugmentCategory == EAugmentCategory::Active
		&& State.OwnedActiveCount >= Rules.MaxActiveLines)
	{
		return false;
	}

	// 재등장 금지는 매치 전체, 레벨업은 예외, 현재는 동작 X
	if (!bIsLevelUp && State.SeenDefinitions.Contains(Definition.GetPrimaryAssetId()))
	{
		return false;
	}

	return true;
}

// 자격을 통과한 후보만 모아 정렬
void RogueAugmentDraft::BuildCandidateBucket(
	const FAugmentDraftRules& Rules,
	const FAugmentDraftEligibility& State,
	const TArray<UAugmentDefinition*>& Definitions,
	TArray<FAugmentInventoryEntry>& OutBucket)
{
	OutBucket.Reset();

	for (const UAugmentDefinition* Definition : Definitions)
	{
		if (!Definition || !IsCandidateEligible(Rules, State, *Definition))
		{
			continue;
		}

		OutBucket.Add(FAugmentInventoryEntry::Make(*Definition, GetOfferLevel(State, *Definition)));
	}

	// 재현성을 위해 이름 순 정렬. 랜덤 추첨은 PickOffers
	OutBucket.Sort([](const FAugmentInventoryEntry& A, const FAugmentInventoryEntry& B)
	{
		return A.DefinitionId.PrimaryAssetName.Compare(B.DefinitionId.PrimaryAssetName) < 0;
	});
}

// 레벨업 1장을 0번 칸에 고정하고 나머지를 균등 추출
// 각 체인 채우는 것을 반복
FAugmentOfferResult RogueAugmentDraft::PickOffers(
	TArray<FAugmentInventoryEntry>& Bucket,
	const int32 CardCount,
	const int32 Seed,
	const int32 CardsPerSlot)
{
	FAugmentOfferResult Result;

	if (CardCount <= 0 || CardsPerSlot <= 0)
	{
		return Result;
	}

	Result.SlotCards.SetNum(CardCount);

	FRandomStream Stream(Seed);

	// 0회차가 제시할 카드, 그 뒤 회차가 리롤 예비
	for (int32 Pass = 0; Pass < CardsPerSlot && !Bucket.IsEmpty(); ++Pass)
	{
		// 이번 회차에 채운 슬롯 수
		int32 Filled = 0;

		// 엔트리 레벨이 2 이상이면 보유 계열의 레벨업
		TArray<int32> LevelUpIndices;
		for (int32 Index = 0; Index < Bucket.Num(); ++Index)
		{
			if (Bucket[Index].Level > 1)
			{
				LevelUpIndices.Add(Index);
			}
		}

		// 레벨업이 가능한 증강 중 랜덤으로 선택
		// 0번 칸에 고정하고 나머지는 균등 추첨
		if (!LevelUpIndices.IsEmpty())
		{
			const int32 Picked = LevelUpIndices[Stream.RandRange(0, LevelUpIndices.Num() - 1)];

			Result.SlotCards[Filled++].Add(Bucket[Picked]);

			if (Pass == 0)
			{
				Result.bLevelUpSlotReserved = true;
			}

			// 뽑은 것은 버킷에서 제거
			Bucket.RemoveAt(Picked);
		}

		// 나머지 슬롯은 균등 추첨, 뽑은 것은 버킷에서 제거
		while (Filled < CardCount && !Bucket.IsEmpty())
		{
			const int32 Picked = Stream.RandRange(0, Bucket.Num() - 1);

			Result.SlotCards[Filled++].Add(Bucket[Picked]);
			Bucket.RemoveAt(Picked);
		}
	}

	// 빈 슬롯 제거
	Result.SlotCards.RemoveAll([](const TArray<FAugmentInventoryEntry>& Chain)
	{
		return Chain.IsEmpty();
	});

	// 뽑을 증강이 없는 경우, 제시하지 못한 슬롯 수를 UnfilledSlots 에 기록
	Result.UnfilledSlots = CardCount - Result.SlotCards.Num();

	return Result;
}
