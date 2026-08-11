// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueAbilitySet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RogueAbilitySet"

#if WITH_EDITOR
namespace
{
	bool RogueAbilitySet_HasActivationGate(const UClass* AbilityClass)
	{
		static const FName GateNames[] = { TEXT("ActivationRequiredTags"), TEXT("ActivationBlockedTags") };

		const UObject* CDO = AbilityClass->GetDefaultObject();
		if (!CDO)
		{
			return true;   // 판단 불가면 통과
		}

		for (const FName& GateName : GateNames)
		{
			const FStructProperty* Property =
				FindFProperty<FStructProperty>(UGameplayAbility::StaticClass(), GateName);
			if (!Property)
			{
				continue;
			}

			const FGameplayTagContainer* Tags = Property->ContainerPtrToValuePtr<FGameplayTagContainer>(CDO);
			if (Tags && !Tags->IsEmpty())
			{
				return true;
			}
		}

		return false;
	}
}
#endif

#if WITH_EDITOR
EDataValidationResult URogueAbilitySet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TMap<FGameplayTag, int32> UngatedCountByInputTag;
	TSet<const UClass*> SeenAbilityClasses;

	for (int32 Index = 0; Index < Abilities.Num(); ++Index)
	{
		const FRogueGrantedAbility& Granted = Abilities[Index];

		if (!Granted.AbilityClass)
		{
			Context.AddError(FText::Format(
				LOCTEXT("NullAbilityClass", "Abilities[{0}] 의 AbilityClass 가 비어 있습니다"), Index));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		// 같은 클래스를 두 번 부여하면 스펙이 둘 생겨 한 번의 입력에 두 번 발동
		if (SeenAbilityClasses.Contains(Granted.AbilityClass))
		{
			Context.AddError(FText::Format(
				LOCTEXT("DuplicateAbilityClass", "Abilities[{0}] 의 '{1}' 이 이미 등록돼 있습니다"),
				Index, FText::FromString(Granted.AbilityClass->GetName())));
			Result = EDataValidationResult::Invalid;
		}
		SeenAbilityClasses.Add(Granted.AbilityClass);

		if (!Granted.InputTag.IsValid())
		{
			continue;
		}

		if (!RogueAbilitySet_HasActivationGate(Granted.AbilityClass))
		{
			UngatedCountByInputTag.FindOrAdd(Granted.InputTag)++;
		}
	}

	for (const TPair<FGameplayTag, int32>& Pair : UngatedCountByInputTag)
	{
		if (Pair.Value > 1)
		{
			Context.AddError(FText::Format(
				LOCTEXT("UngatedSharedInputTag",
					"입력 슬롯 '{0}' 에 ActivationRequiredTags 가 없는 어빌리티가 {1}개입니다. "
					"한 번 누르면 전부 켜집니다 - 장착 태그 등으로 갈라주세요"),
				FText::FromString(Pair.Key.ToString()), Pair.Value));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
