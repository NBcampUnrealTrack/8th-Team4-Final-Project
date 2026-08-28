// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentDefinition.h"

#include "Tags/GameplayTags.h"

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

FGameplayTag UAugmentDefinition::GetActiveInputSlotTag() const
{
	for (const FAugmentGrantedAbility& Granted : GrantedAbilities)
	{
		if (Granted.InputTag.IsValid())
		{
			return Granted.InputTag;
		}
	}

	return FGameplayTag();
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

		if (Rule.bDistanceScalesWithRangeBonus && !Rule.bUseDistanceRange)
		{
			Context.AddWarning(FText::Format(
				LOCTEXT("RangeScaleWithoutDistance",
					"{0}: 사거리 스케일을 켰지만 거리 조건이 꺼져 있습니다. 이 설정은 무시됩니다"),
				FText::FromString(Where)));
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

		// 흡혈은 주는 피해 규칙에서만 산출
		if (Rule.Side == EAugmentDamageRuleSide::Taken && !FMath::IsNearlyZero(Rule.LifestealRate))
		{
			Context.AddWarning(FText::Format(
				LOCTEXT("TakenRuleLifesteal",
					"{0}: 받는 피해 규칙의 흡혈률은 무시됩니다. 흡혈은 주는 피해 규칙에만 넣으세요"),
				FText::FromString(Where)));
		}

		// 조건만 있고 효과가 없는 규칙
		if (FMath::IsNearlyZero(Rule.DamageBonusRate)
			&& FMath::IsNearlyZero(Rule.FlatDamageBonus)
			&& FMath::IsNearlyZero(Rule.LifestealRate))
		{
			Context.AddWarning(FText::Format(
				LOCTEXT("NoEffectRule", "{0}: 증감률, 고정값, 흡혈률이 모두 0 입니다. 이 규칙은 아무 효과가 없습니다"),
				FText::FromString(Where)));
		}

		return bValid;
	}

	// 레벨 공통으로 부여하는 GE / GA / 대미지
	bool AreSharedEffectsValid(const UAugmentDefinition& Definition, FDataValidationContext& Context)
	{
		bool bValid = true;

		const FAugmentLevelData* LevelData = Definition.GetLevelData();
		const bool bHasDamage = LevelData
			&& LevelData->SetByCallerMagnitudes.FindRef(RogueTags::Data_Magnitude_Damage) > 0.f;

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
		int32 SlotInputCount = 0;

		// 트리거 어빌리티는 자기 클래스로 GrantedAbilities 항목을 찾는다. 같은 클래스가 두 번이면 구분 불가
		TSet<FSoftObjectPath> UsedAbilityClasses;

		for (int32 Index = 0; Index < Definition.GrantedAbilities.Num(); ++Index)
		{
			const FAugmentGrantedAbility& Granted = Definition.GrantedAbilities[Index];

			if (Granted.AbilityClass.IsNull())
			{
				Context.AddError(FText::Format(
					LOCTEXT("NullAbilityClass", "GrantedAbilities[{0}] 의 AbilityClass 가 비어 있습니다"), Index));
				bValid = false;
			}
			else
			{
				const FSoftObjectPath ClassPath = Granted.AbilityClass.ToSoftObjectPath();

				if (UsedAbilityClasses.Contains(ClassPath))
				{
					Context.AddError(FText::Format(
						LOCTEXT("DuplicateAbilityClass",
							"GrantedAbilities[{0}] 의 AbilityClass '{1}' 이 중복입니다. "
							"트리거 어빌리티가 어느 항목의 설정을 써야 할지 정할 수 없습니다"),
						Index, FText::FromString(ClassPath.GetAssetName())));
					bValid = false;
				}

				UsedAbilityClasses.Add(ClassPath);
			}

			// 트리거 거리 조건
			if (Granted.bUseDistanceRange && Granted.MinDistanceMeters > Granted.MaxDistanceMeters)
			{
				Context.AddError(FText::Format(
					LOCTEXT("TriggerBadDistanceRange",
						"GrantedAbilities[{0}] 의 거리 범위가 뒤집혀 있습니다"), Index));
				bValid = false;
			}

			if (Granted.bUseDistanceRange && Granted.MaxDistanceMeters <= 0.f)
			{
				Context.AddError(FText::Format(
					LOCTEXT("TriggerZeroDistanceMax",
						"GrantedAbilities[{0}] 가 거리 조건을 켜고 최대 거리를 채우지 않았습니다. "
						"이 트리거는 영영 발동하지 않습니다 (상한이 없으면 큰 수를 넣으세요)"), Index));
				bValid = false;
			}

			if (Granted.bDistanceScalesWithRangeBonus && !Granted.bUseDistanceRange)
			{
				Context.AddWarning(FText::Format(
					LOCTEXT("TriggerRangeScaleWithoutDistance",
						"GrantedAbilities[{0}] 가 사거리 스케일을 켰지만 거리 조건이 꺼져 있습니다. "
						"이 설정은 무시됩니다"), Index));
			}

			// 입력, GE, 대미지가 전부 없는 경우
			if (!Granted.InputTag.IsValid() && Granted.EffectsToApply.IsEmpty() && !bHasDamage)
			{
				Context.AddWarning(FText::Format(
					LOCTEXT("TriggerWithoutEffects",
						"GrantedAbilities[{0}] 에 입력 슬롯도 EffectsToApply 도 없습니다. "
						"이벤트 트리거 증강이라면 EffectsToApply 나 {1} 을 채우세요"),
					Index, FText::FromString(RogueTags::Data_Magnitude_Damage.GetTag().ToString())));
			}

			for (int32 EffectIndex = 0; EffectIndex < Granted.EffectsToApply.Num(); ++EffectIndex)
			{
				if (Granted.EffectsToApply[EffectIndex].IsNull())
				{
					Context.AddError(FText::Format(
						LOCTEXT("NullTriggerEffect",
							"GrantedAbilities[{0}] 의 EffectsToApply[{1}] 가 비어 있습니다"), Index, EffectIndex));
					bValid = false;
				}
			}

			if (!Granted.InputTag.IsValid())
			{
				continue;
			}

			++SlotInputCount;

			if (UsedInputTags.HasTagExact(Granted.InputTag))
			{
				Context.AddError(FText::Format(
					LOCTEXT("DuplicateInputTag", "GrantedAbilities[{0}] 의 입력 슬롯 '{1}' 이 중복입니다. 뒤쪽 어빌리티는 발동하지 않습니다"),
					Index, FText::FromString(Granted.InputTag.ToString())));
				bValid = false;
			}

			UsedInputTags.AddTag(Granted.InputTag);
		}

		// Active 계열이면 입력 슬롯이 지정된 어빌리티가 반드시 1개 있어야함
		const int32 ExpectedSlotCount = Definition.AugmentCategory == EAugmentCategory::Active ? 1 : 0;
		if (SlotInputCount != ExpectedSlotCount)
		{
			Context.AddError(FText::Format(
				LOCTEXT("SlotInputCount", "InputTag 가 지정된 어빌리티가 {0}개여야 하는데 {1}개입니다"),
				ExpectedSlotCount, SlotInputCount));
			bValid = false;
		}

		return bValid;
	}

	bool IsLevelValid(const FAugmentLevelData& LevelData,
		bool bHasSharedEffects, FDataValidationContext& Context)
	{
		bool bValid = true;

		// 공통 GE/GA 가 없는 경우
		if (!bHasSharedEffects && LevelData.ConditionalDamageRules.IsEmpty())
		{
			Context.AddError(LOCTEXT("EmptyLevel",
				"조건부 규칙이 없습니다. 공통 GE / GA 도 없어 획득해도 아무 효과가 없습니다"));
			bValid = false;
		}

		for (int32 RuleIndex = 0; RuleIndex < LevelData.ConditionalDamageRules.Num(); ++RuleIndex)
		{
			const FString Where = FString::Printf(TEXT("ConditionalDamageRules[%d]"), RuleIndex);

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

	// EAugmentCategory::None인 경우, 기존의 Stat 자리가 삭제됨
	if (AugmentCategory == EAugmentCategory::None)
	{
		Context.AddError(LOCTEXT("NoCategory",
			"AugmentCategory 가 비어 있습니다. 기획 시트의 '타입(Category)' 열에 맞춰 고르세요"));
		MarkInvalid();
	}

	// 레벨업이 없으므로 수치는 정확히 한 칸
	if (Levels.Num() != 1)
	{
		Context.AddError(FText::Format(
			LOCTEXT("LevelCount", "Levels 는 1개여야 하는데 {0}개입니다"), Levels.Num()));
		MarkInvalid();
	}

	if (!AreSharedEffectsValid(*this, Context))
	{
		MarkInvalid();
	}

	const FAugmentLevelData* LevelData = GetLevelData();
	if (LevelData == nullptr)
	{
		return Result;
	}

	const bool bHasSharedEffects = !GrantedEffects.IsEmpty() || !GrantedAbilities.IsEmpty();
	if (!bHasSharedEffects && LevelData->ConditionalDamageRules.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoEffects",
			"GE / GA 가 없고 조건부 규칙도 없습니다. 획득해도 아무 효과가 없습니다"));
		MarkInvalid();

		return Result;
	}

	if (!IsLevelValid(*LevelData, bHasSharedEffects, Context))
	{
		MarkInvalid();
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
