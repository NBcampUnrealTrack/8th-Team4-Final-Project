// Fill out your copyright notice in the Description page of Project Settings.

#include "Augment/RogueAugmentStatResolver.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/RogueAugmentAttributeSet.h"

namespace
{
	// 나눗셈과 제곱근에 들어가는 배율의 하한.
	// 밸런스 상한이 아니라 연산 안전장치다 — 0 이 들어오면 inf/NaN 이 되어 전파된다.
	constexpr float MinMultiplier = 0.01f;
}

const URogueAugmentAttributeSet* URogueAugmentStatResolver::FindAugmentSet(const AActor* Actor)
{
	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Actor));

	return ASC ? ASC->GetSet<URogueAugmentAttributeSet>() : nullptr;
}

// 전투 관련
float URogueAugmentStatResolver::GetFinalRPM(const AActor* Actor, float BaseRPM)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BaseRPM;
	}

	return BaseRPM * FMath::Max(MinMultiplier, Set->GetFireRateMultiplier());
}

float URogueAugmentStatResolver::GetFinalReloadTime(const AActor* Actor, float BaseReloadTime)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BaseReloadTime;
	}

	// 속도 배율이므로 시간은 나눈다
	return BaseReloadTime / FMath::Max(MinMultiplier, Set->GetReloadSpeedMultiplier());
}

int32 URogueAugmentStatResolver::GetFinalMagazineSize(const AActor* Actor, int32 BaseMagazineSize)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BaseMagazineSize;
	}

	const float Scaled = BaseMagazineSize * FMath::Max(MinMultiplier, Set->GetMagazineSizeMultiplier());

	// 탄창 0 이면 사격 자체가 불가능, 최소 1발은 유지
	return FMath::Max(1, FMath::FloorToInt(Scaled));
}

int32 URogueAugmentStatResolver::GetFinalPelletCount(const AActor* Actor, int32 BasePelletCount)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BasePelletCount;
	}

	return FMath::Max(1, BasePelletCount + FMath::RoundToInt(Set->GetPelletCountBonus()));
}

float URogueAugmentStatResolver::GetFinalMeleeRange(const AActor* Actor, float BaseRangeMeters)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BaseRangeMeters;
	}

	return BaseRangeMeters + Set->GetMeleeRangeBonus();
}

float URogueAugmentStatResolver::GetFinalSpread(const AActor* Actor, float BaseSpread)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BaseSpread;
	}

	return BaseSpread * Set->GetSpreadMultiplier();
}

float URogueAugmentStatResolver::GetFinalRecoil(const AActor* Actor, float BaseRecoil)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BaseRecoil;
	}

	return BaseRecoil * Set->GetRecoilMultiplier();
}

// 이동 관련
float URogueAugmentStatResolver::GetFinalMoveSpeed(const AActor* Actor, float BaseSpeed)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BaseSpeed;
	}

	return BaseSpeed * Set->GetMoveSpeedMultiplier();
}

float URogueAugmentStatResolver::GetFinalSprintSpeed(const AActor* Actor, float BaseSprintSpeed)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BaseSprintSpeed;
	}

	// 달리기 속도는 이동 속도와 별개로 배율을 적용
	return BaseSprintSpeed * Set->GetSprintSpeedMultiplier();
}

float URogueAugmentStatResolver::GetFinalJumpVelocity(const AActor* Actor, float BaseJumpVelocity)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BaseJumpVelocity;
	}

	// 증강은 '높이' 배율인데 CMC 는 속도 기준
	// 높이는 속도의 제곱에 비례하므로 제곱근
	return BaseJumpVelocity * FMath::Sqrt(FMath::Max(MinMultiplier, Set->GetJumpHeightMultiplier()));
}

int32 URogueAugmentStatResolver::GetExtraJumpCount(const AActor* Actor)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return 0;
	}

	return FMath::Max(0, FMath::RoundToInt(Set->GetExtraJumpCount()));
}


// 증강 관련
float URogueAugmentStatResolver::GetFinalCooldown(const AActor* Actor, float BaseCooldownSeconds)
{
	const URogueAugmentAttributeSet* Set = FindAugmentSet(Actor);
	if (!Set)
	{
		return BaseCooldownSeconds;
	}

	const float Final = BaseCooldownSeconds * (1.f - Set->GetCooldownReductionRate());

	// 쿨타임 음수 방지
	return FMath::Max(0.f, Final);
}
