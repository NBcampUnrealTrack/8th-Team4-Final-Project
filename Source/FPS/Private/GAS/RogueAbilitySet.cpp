// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueAbilitySet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RogueAbilitySet"

#if WITH_EDITOR
EDataValidationResult URogueAbilitySet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 하나의 입력 태그당 하나의 어빌리티만 부여
	FGameplayTagContainer UsedInputTags;

	for (int32 Index = 0; Index < Abilities.Num(); ++Index)
	{
		const FRogueGrantedAbility& Granted = Abilities[Index];

		if (!Granted.AbilityClass)
		{
			Context.AddError(FText::Format(
				LOCTEXT("NullAbilityClass", "Abilities[{0}] 의 AbilityClass 가 비어 있습니다"), Index));
			Result = EDataValidationResult::Invalid;
		}

		if (!Granted.InputTag.IsValid())
		{
			continue;
		}

		if (UsedInputTags.HasTagExact(Granted.InputTag))
		{
			Context.AddError(FText::Format(
				LOCTEXT("DuplicateInputTag", "Abilities[{0}] 의 입력 슬롯 '{1}' 이 중복입니다"),
				Index, FText::FromString(Granted.InputTag.ToString())));
			Result = EDataValidationResult::Invalid;
		}

		UsedInputTags.AddTag(Granted.InputTag);
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
