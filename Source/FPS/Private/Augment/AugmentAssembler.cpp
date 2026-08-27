// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentAssembler.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/Abilities/RogueGameplayAbility.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentAssembler, Log, All);

namespace
{
	// 소프트 클래스 로드, 실패하면 어느 칸인지 남기고 nullptr
	template <typename T>
	UClass* LoadGrantedClass(const TSoftClassPtr<T>& SoftClass,
		const UAugmentDefinition& Definition, const TCHAR* FieldName, int32 Index)
	{
		UClass* Loaded = SoftClass.LoadSynchronous();
		if (!Loaded)
		{
			UE_LOG(LogAugmentAssembler, Error,
				TEXT("[AugmentAssembler] %s 의 %s[%d] 를 로드하지 못했습니다"),
				*Definition.GetName(), FieldName, Index);
		}

		return Loaded;
	}

	// 해석된 GE 적용 및 해제용 핸들 저장
	void ApplyResolvedEffects(UAbilitySystemComponent& ASC,
		const FAugmentResolvedGrant& Resolved, FAugmentGrantHandles& OutHandles)
	{
		const UAugmentDefinition& Definition = *Resolved.Definition;

		for (int32 Index = 0; Index < Resolved.EffectClasses.Num(); ++Index)
		{
			FGameplayEffectContextHandle Context = ASC.MakeEffectContext();

			// 어느 증강이 건 효과인지
			Context.AddSourceObject(&Definition);

			// 스펙 생성, GE 레벨은 항상 1
			const FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(Resolved.EffectClasses[Index], 1.f, Context);
			if (!Spec.IsValid())
			{
				UE_LOG(LogAugmentAssembler, Error,
					TEXT("[AugmentAssembler] %s 의 GrantedEffects[%d] 스펙 생성에 실패했습니다"),
					*Definition.GetName(), Index);
				continue;
			}

			// 각 레벨의 SetByCallerMagnitude 를 스펙에 적용. Magnitude 는 레벨마다 다르게 설정
			for (const TPair<FGameplayTag, float>& Magnitude : Resolved.LevelData->SetByCallerMagnitudes)
			{
				Spec.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
			}

			// 스펙 적용
			const FActiveGameplayEffectHandle Handle = ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data);
			if (!Handle.IsValid())
			{
				// Instant GE 는 핸들 생성 없이 바로 적용되므로, DurationPolicy 가 Infinite 인지 확인
				UE_LOG(LogAugmentAssembler, Error,
					TEXT("[AugmentAssembler] %s 의 GrantedEffects[%d] 핸들이 무효입니다. ")
					TEXT("Duration Policy 가 Infinite 인지, 태그로 차단되지 않았는지 확인하세요"),
					*Definition.GetName(), Index);
				continue;
			}

			// 핸들 저장
			OutHandles.EffectHandles.Add(Handle);
		}
	}

	// 해석된 GA 를 부여 해제용 핸들 저장
	void GiveResolvedAbilities(UAbilitySystemComponent& ASC,
		const FAugmentResolvedGrant& Resolved, FAugmentGrantHandles& OutHandles)
	{
		const UAugmentDefinition& Definition = *Resolved.Definition;

		for (int32 Index = 0; Index < Resolved.Abilities.Num(); ++Index)
		{
			const FAugmentResolvedAbility& Ability = Resolved.Abilities[Index];

			// 어느 증강이 준 어빌리티인지
			UObject* SourceObject = const_cast<UAugmentDefinition*>(&Definition);

			FGameplayAbilitySpec Spec(Ability.AbilityClass, 1, INDEX_NONE, SourceObject);

			if (Ability.InputTag.IsValid())
			{
				// 입력 태그 부여
				Spec.GetDynamicSpecSourceTags().AddTag(Ability.InputTag);
			}

			// 어빌리티 부여
			const FGameplayAbilitySpecHandle Handle = ASC.GiveAbility(Spec);
			if (!Handle.IsValid())
			{
				UE_LOG(LogAugmentAssembler, Error,
					TEXT("[AugmentAssembler] %s 의 GrantedAbilities[%d] 부여에 실패했습니다"),
					*Definition.GetName(), Index);
				continue;
			}

			// 핸들 저장
			OutHandles.AbilityHandles.Add(Handle);
		}
	}
}

// 증강 정의 해석, GE/GA 로드
bool RogueAugmentAssembler::ResolveAugment(
	const UAugmentDefinition& Definition,
	FAugmentResolvedGrant& OutResolved)
{
	OutResolved = FAugmentResolvedGrant();

	// 수치 데이터 검사
	const FAugmentLevelData* LevelData = Definition.GetLevelData();
	if (!LevelData)
	{
		UE_LOG(LogAugmentAssembler, Error,
			TEXT("[AugmentAssembler] %s 에 Levels 가 비어 있습니다"), *Definition.GetName());
		return false;
	}

	OutResolved.Definition = &Definition;

	bool bResolved = true;

	// GE 로드
	OutResolved.EffectClasses.Reserve(Definition.GrantedEffects.Num());
	for (int32 Index = 0; Index < Definition.GrantedEffects.Num(); ++Index)
	{
		UClass* EffectClass =
			LoadGrantedClass(Definition.GrantedEffects[Index], Definition, TEXT("GrantedEffects"), Index);

		if (!EffectClass)
		{
			bResolved = false;
			continue;
		}

		OutResolved.EffectClasses.Add(EffectClass);
	}

	// GA 로드
	OutResolved.Abilities.Reserve(Definition.GrantedAbilities.Num());
	for (int32 Index = 0; Index < Definition.GrantedAbilities.Num(); ++Index)
	{
		const FAugmentGrantedAbility& Granted = Definition.GrantedAbilities[Index];

		UClass* AbilityClass =
			LoadGrantedClass(Granted.AbilityClass, Definition, TEXT("GrantedAbilities"), Index);

		if (!AbilityClass)
		{
			bResolved = false;
			continue;
		}

		OutResolved.Abilities.Add({ AbilityClass, Granted.InputTag });
	}

	// 하나라도 실패하면 해석 자체를 실패
	if (!bResolved)
	{
		return false;
	}

	// 레벨 데이터 저장
	OutResolved.LevelData = LevelData;

	return true;
}

// 해석된 증강을 ASC에 적용
FAugmentGrantHandles RogueAugmentAssembler::ApplyResolved(UAbilitySystemComponent& ASC,
	const FAugmentResolvedGrant& Resolved)
{
	FAugmentGrantHandles Handles;

	// 증강 부여는 서버에서
	if (!ASC.IsOwnerActorAuthoritative())
	{
		UE_LOG(LogAugmentAssembler, Error,
			TEXT("[AugmentAssembler] 권한이 없는 곳에서 %s 를 부여하려 했습니다"),
			*GetNameSafe(Resolved.Definition));
		return Handles;
	}

	if (!Resolved.IsValid())
	{
		UE_LOG(LogAugmentAssembler, Error,
			TEXT("[AugmentAssembler] %s 를 해석하지 않고 부여하려 했습니다. ResolveAugment 를 먼저 호출하세요"),
			*GetNameSafe(Resolved.Definition));
		return Handles;
	}

	// GE/GA 부여
	ApplyResolvedEffects(ASC, Resolved, Handles);
	GiveResolvedAbilities(ASC, Resolved, Handles);

	return Handles;
}

// ASC에서 증강 제거
void RogueAugmentAssembler::RemoveAugment(UAbilitySystemComponent& ASC, FAugmentGrantHandles& Handles)
{
	if (!ASC.IsOwnerActorAuthoritative())
	{
		UE_LOG(LogAugmentAssembler, Error, TEXT("[AugmentAssembler] 권한이 없는 곳에서 증강을 해제하려 했습니다"));
		return;
	}

	for (const FActiveGameplayEffectHandle& Handle : Handles.EffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC.RemoveActiveGameplayEffect(Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : Handles.AbilityHandles)
	{
		if (Handle.IsValid())
		{
			// 증강이 부여한 어빌리티 제거
			ASC.ClearAbility(Handle);
		}
	}

	// 핸들 리셋
	Handles.Reset();
}
