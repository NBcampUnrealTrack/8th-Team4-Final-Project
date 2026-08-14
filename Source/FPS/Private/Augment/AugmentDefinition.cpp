// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentDefinition.h"

#include "Augment/RogueGameplayAbility.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "AugmentDefinition"

// 클래스명이 아니라 고정 문자열 사용
// AssetManager 에 등록하는 타입 이름. (PrimaryAssetType)
const FPrimaryAssetType UAugmentDefinition::AugmentAssetType("Augment");

FPrimaryAssetId UAugmentDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AugmentAssetType, GetFName());
}

#if WITH_EDITOR

namespace
{
	bool IsRuleValid(const FAugmentConditionalDamageRule& Rule, const FString& Where, FDataValidationContext& Context)
	{
		bool bValid = true;

		auto Reject = [&](const FText& Reason)
		{
			Context.AddError(FText::Format(LOCTEXT("BadRule", "{0}: {1}"), FText::FromString(Where), Reason));
			bValid = false;
		};

		if (Rule.bUseDistanceRange && Rule.MinDistanceMeters > Rule.MaxDistanceMeters)
		{
			Reject(LOCTEXT("BadDistanceRange", "거리 범위가 뒤집혀 있습니다"));
		}

		if (Rule.bUseDistanceRange && Rule.MaxDistanceMeters <= 0.f)
		{
			Reject(LOCTEXT("ZeroDistanceMax",
				"거리 조건을 켜고 최대 거리를 채우지 않았습니다. 이 규칙은 영영 발동하지 않습니다 (상한이 없으면 큰 수를 넣으세요)"));
		}

		if (Rule.bUseTargetHealthRange && Rule.TargetHealthMinPct > Rule.TargetHealthMaxPct)
		{
			Reject(LOCTEXT("BadTargetHealthRange", "피해자 체력 범위가 뒤집혀 있습니다"));
		}

		if (Rule.bUseSourceHealthRange && Rule.SourceHealthMinPct > Rule.SourceHealthMaxPct)
		{
			Reject(LOCTEXT("BadSourceHealthRange", "가해자 체력 범위가 뒤집혀 있습니다"));
		}

		if (Rule.bUseShotIndexRange && Rule.MinShotIndexInMagazine > Rule.MaxShotIndexInMagazine)
		{
			Reject(LOCTEXT("BadShotIndexRange", "탄창 인덱스 범위가 뒤집혀 있습니다"));
		}

		// Required 와 Blocked 태그가 겹치면 발동 조건이 모순
		if (Rule.RequiredContextTags.HasAny(Rule.BlockedContextTags))
		{
			Reject(LOCTEXT("ContradictoryTags", "Required 와 Blocked 태그가 겹칩니다"));
		}

		return bValid;
	}

	// 레벨 공통으로 부여하는 GE / GA
	bool AreSharedEffectsValid(const UAugmentDefinition& Definition, FDataValidationContext& Context)
	{
		bool bValid = true;

		for (int32 Index = 0; Index < Definition.GrantedEffects.Num(); ++Index)
		{
			if (Definition.GrantedEffects[Index].IsNull())
			{
				Context.AddError(FText::Format(
					LOCTEXT("NullEffectClass", "GrantedEffects[{0}] 가 비어 있습니다"), Index));
				bValid = false;
			}
		}

		// GA 입력 슬롯 중복 체크
		FGameplayTagContainer UsedInputTags;
		bool bHasAnyInput = false;

		for (int32 Index = 0; Index < Definition.GrantedAbilities.Num(); ++Index)
		{
			const FAugmentGrantedAbility& Granted = Definition.GrantedAbilities[Index];

			if (Granted.AbilityClass.IsNull())
			{
				Context.AddError(FText::Format(
					LOCTEXT("NullAbilityClass", "GrantedAbilities[{0}] 의 AbilityClass 가 비어 있습니다"), Index));
				bValid = false;
			}

			if (!Granted.InputTag.IsValid())
			{
				continue;
			}

			bHasAnyInput = true;

			if (UsedInputTags.HasTagExact(Granted.InputTag))
			{
				Context.AddError(FText::Format(
					LOCTEXT("DuplicateInputTag", "GrantedAbilities[{0}] 의 입력 슬롯 '{1}' 이 중복입니다. 뒤쪽 어빌리티는 발동하지 않습니다"),
					Index, FText::FromString(Granted.InputTag.ToString())));
				bValid = false;
			}

			UsedInputTags.AddTag(Granted.InputTag);
		}

		// 액티브 스킬인데 발동 태그가 없으면 경고
		if (Definition.AugmentCategory == EAugmentCategory::Active
			&& !Definition.GrantedAbilities.IsEmpty() && !bHasAnyInput)
		{
			Context.AddWarning(
				LOCTEXT("ActiveWithoutInput", "액티브 증강인데 입력 슬롯이 지정된 어빌리티가 없습니다. 이벤트로 발동하는 것이 아니라면 InputTag 를 지정하세요"));
		}

		return bValid;
	}

	bool IsLevelValid(const UAugmentDefinition& Definition, int32 LevelIndex, FDataValidationContext& Context)
	{
		const FAugmentLevelData& LevelData = Definition.Levels[LevelIndex];
		const int32 LevelNumber = LevelIndex + 1;

		bool bValid = true;

		// GE, GA, 조건부 규칙이 모두 없는 레벨은 의미가 없음
		if (Definition.GrantedEffects.IsEmpty() && Definition.GrantedAbilities.IsEmpty()
			&& LevelData.ConditionalDamageRules.IsEmpty())
		{
			Context.AddError(FText::Format(
				LOCTEXT("EmptyLevel", "{0}레벨에 GE / GA / 조건부 규칙이 모두 없습니다. 획득해도 아무 효과가 없습니다"),
				LevelNumber));
			bValid = false;
		}

		for (int32 RuleIndex = 0; RuleIndex < LevelData.ConditionalDamageRules.Num(); ++RuleIndex)
		{
			const FString Where = FString::Printf(
				TEXT("%d레벨 ConditionalDamageRules[%d]"), LevelNumber, RuleIndex);

			if (!IsRuleValid(LevelData.ConditionalDamageRules[RuleIndex], Where, Context))
			{
				bValid = false;
			}
		}

		return bValid;
	}
}

EDataValidationResult UAugmentDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	auto MarkInvalid = [&Result]() { Result = EDataValidationResult::Invalid; };

	// 증강 계열이 없는 경우
	if (!LineTag.IsValid())
	{
		Context.AddError(LOCTEXT("NoLineTag", "LineTag 가 비어 있습니다. 계열이 없으면 상점 후보에서 제외됩니다"));
		MarkInvalid();
	}

	// 레벨 배열이 빈 증강은 상점 후보에서 제외
	if (Levels.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoLevels", "Levels 가 비어 있습니다. 최소 1레벨은 있어야 상점에 나옵니다"));
		MarkInvalid();
	}

	if (!AreSharedEffectsValid(*this, Context))
	{
		MarkInvalid();
	}

	for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); ++LevelIndex)
	{
		if (!IsLevelValid(*this, LevelIndex, Context))
		{
			MarkInvalid();
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
