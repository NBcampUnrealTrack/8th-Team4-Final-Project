// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentDraft.h"

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
