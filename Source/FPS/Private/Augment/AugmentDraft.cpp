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

bool RogueAugmentDraft::MeetsUnlockRequirements(
    const UAugmentDefinition& Definition, const FGameplayTagContainer& UnlockTags)
{
    return UnlockTags.HasAll(Definition.RequiredUnlockTags);
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

	const bool bIsActive = Definition.AugmentCategory == EAugmentCategory::Active;

	// 풀에 맞지 않는 경우
	if ((State.Pool == EAugmentRewardPool::Passive && bIsActive)
		|| (State.Pool == EAugmentRewardPool::Active && !bIsActive))
	{
		return false;
	}

	// 등급이 다른 경우, 액티브는 슬롯 기준으로 하므로 건너뜀
	if (State.Pool != EAugmentRewardPool::Active && Definition.Tier != State.Tier)
	{
		return false;
	}

	// 수치가 없는 경우
	if (Definition.GetLevelData() == nullptr)
	{
		return false;
	}

	// 이미 보유한 계열인 경우
	const bool bIsOwned = State.Owned.ContainsByPredicate(
		[&Definition](const FAugmentInventoryEntry& Entry)
		{
			return Entry.LineTag.MatchesTagExact(Definition.LineTag);
		});

	if (bIsOwned)
	{
		return false;
	}

	// 필요한 태그가 없는 경우
	if (!MeetsUnlockRequirements(Definition, State.UnlockTags))
	{
		return false;
	}

	// 인벤토리가 가득 찬 경우
	if (Rules.MaxTotalLines > 0 && State.Owned.Num() >= Rules.MaxTotalLines)
	{
		return false;
	}

	// 카테고리별 상한
	const int32 OwnedInCategory = bIsActive
		? State.OwnedActiveCount
		: State.Owned.Num() - State.OwnedActiveCount;
	const int32 CategoryLimit = bIsActive ? Rules.MaxActiveLines : Rules.MaxPassiveLines;

	if (OwnedInCategory >= CategoryLimit)
	{
		return false;
	}

	// 액티브는 애셋이 지정한 슬롯이 비어 있어야 함
	if (bIsActive)
	{
		const FGameplayTag SlotTag = Definition.GetActiveInputSlotTag();
		if (!SlotTag.IsValid() || State.OccupiedActiveSlots.HasTagExact(SlotTag))
		{
			return false;
		}

		if (State.RequiredActiveSlot.IsValid() && !SlotTag.MatchesTagExact(State.RequiredActiveSlot))
		{
			return false;
		}
	}

	// 재등장 금지는 매치 전체
	if (State.SeenDefinitions.Contains(Definition.GetPrimaryAssetId()))
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

		OutBucket.Add(FAugmentInventoryEntry::Make(*Definition));
	}

	// 재현성을 위해 이름 순 정렬. 랜덤 추첨은 PickOffers
	OutBucket.Sort([](const FAugmentInventoryEntry& A, const FAugmentInventoryEntry& B)
	{
		return A.DefinitionId.PrimaryAssetName.Compare(B.DefinitionId.PrimaryAssetName) < 0;
	});
}

// 버킷에서 균등 추출해 각 체인을 채우는 것을 반복
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

		// 균등 추첨, 뽑은 것은 버킷에서 제거
		while (Filled < CardCount && !Bucket.IsEmpty())
		{
			const int32 Picked = Stream.RandRange(0, Bucket.Num() - 1);

			Result.SlotCards[Filled++].Add(Bucket[Picked]);
			Bucket.RemoveAtSwap(Picked, 1, EAllowShrinking::No);
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
