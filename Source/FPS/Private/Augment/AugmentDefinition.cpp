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
EDataValidationResult UAugmentDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 증강 계열이 없는 경우
	if (!LineTag.IsValid())
	{
		Context.AddError(LOCTEXT("NoLineTag", "LineTag 가 비어 있습니다. 계열이 없으면 상점 후보에서 제외됩니다"));
		Result = EDataValidationResult::Invalid;
	}

	if (Level < 1)
	{
		Context.AddError(LOCTEXT("BadLevel", "Level 은 1 이상이어야 합니다"));
		Result = EDataValidationResult::Invalid;
	}

	// 모든 항목이 비어있는 경우
	if (GrantedEffects.IsEmpty() && GrantedAbilities.IsEmpty() && ConditionalDamageRules.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoEffect", "GE / GA / 조건부 규칙이 모두 비어 있습니다. 획득해도 아무 효과가 없습니다"));
		Result = EDataValidationResult::Invalid;
	}

	// 증강 효과가 비어있는 경우
	for (int32 Index = 0; Index < GrantedEffects.Num(); ++Index)
	{
		if (GrantedEffects[Index].EffectClass.IsNull())
		{
			Context.AddError(FText::Format(
				LOCTEXT("NullEffectClass", "GrantedEffects[{0}] 의 EffectClass 가 비어 있습니다"), Index));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 같은 증강 안에서 입력 슬롯이 겹치면 뒤쪽 어빌리티는 영영 발동하지 않는다
	FGameplayTagContainer UsedInputTags;
	bool bHasAnyInput = false;

	for (int32 Index = 0; Index < GrantedAbilities.Num(); ++Index)
	{
		const FAugmentGrantedAbility& Granted = GrantedAbilities[Index];

		if (Granted.AbilityClass.IsNull())
		{
			Context.AddError(FText::Format(
				LOCTEXT("NullAbilityClass", "GrantedAbilities[{0}] 의 AbilityClass 가 비어 있습니다"), Index));
			Result = EDataValidationResult::Invalid;
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
			Result = EDataValidationResult::Invalid;
		}

		UsedInputTags.AddTag(Granted.InputTag);
	}

	// 액티브 스킬인데 발동 태그가 없으면 경고
	if (AugmentCategory == EAugmentCategory::Active && !GrantedAbilities.IsEmpty() && !bHasAnyInput)
	{
		Context.AddWarning(
			LOCTEXT("ActiveWithoutInput", "액티브 증강인데 입력 슬롯이 지정된 어빌리티가 없습니다. 이벤트로 발동하는 것이 아니라면 InputTag 를 지정하세요"));
	}

	// 조건부 대미지 규칙 검증
	for (int32 Index = 0; Index < ConditionalDamageRules.Num(); ++Index)
	{
		const FAugmentConditionalDamageRule& Rule = ConditionalDamageRules[Index];

		if (Rule.bUseDistanceRange && Rule.MinDistanceMeters > Rule.MaxDistanceMeters)
		{
			Context.AddError(FText::Format(
				LOCTEXT("BadDistanceRange", "ConditionalDamageRules[{0}] 의 거리 범위가 뒤집혀 있습니다"), Index));
			Result = EDataValidationResult::Invalid;
		}

		if (Rule.bUseTargetHealthRange && Rule.TargetHealthMinPct > Rule.TargetHealthMaxPct)
		{
			Context.AddError(FText::Format(
				LOCTEXT("BadTargetHealthRange", "ConditionalDamageRules[{0}] 의 피해자 체력 범위가 뒤집혀 있습니다"), Index));
			Result = EDataValidationResult::Invalid;
		}

		if (Rule.bUseSourceHealthRange && Rule.SourceHealthMinPct > Rule.SourceHealthMaxPct)
		{
			Context.AddError(FText::Format(
				LOCTEXT("BadSourceHealthRange", "ConditionalDamageRules[{0}] 의 가해자 체력 범위가 뒤집혀 있습니다"), Index));
			Result = EDataValidationResult::Invalid;
		}

		if (Rule.bUseShotIndexRange && Rule.MinShotIndexInMagazine > Rule.MaxShotIndexInMagazine)
		{
			Context.AddError(FText::Format(
				LOCTEXT("BadShotIndexRange", "ConditionalDamageRules[{0}] 의 탄창 인덱스 범위가 뒤집혀 있습니다"), Index));
			Result = EDataValidationResult::Invalid;
		}

		// Required 와 Blocked 태그가 겹치면 발동 조건이 모순
		if (Rule.RequiredContextTags.HasAny(Rule.BlockedContextTags))
		{
			Context.AddError(FText::Format(
				LOCTEXT("ContradictoryTags", "ConditionalDamageRules[{0}] 의 Required 와 Blocked 태그가 겹칩니다"), Index));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
