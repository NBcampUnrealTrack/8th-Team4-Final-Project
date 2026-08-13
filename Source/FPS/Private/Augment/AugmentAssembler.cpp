// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentAssembler.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/RogueGameplayAbility.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentAssembler, Log, All);

namespace RogueAugmentAssembler
{
	namespace
	{
		void ApplyGrantedEffects(UAbilitySystemComponent& ASC,
		                         const UAugmentDefinition& Definition,
		                         const FAugmentLevelData& LevelData,
		                         FAugmentGrantHandles& OutHandles)
		{
			for (int32 Index = 0; Index < Definition.GrantedEffects.Num(); ++Index)
			{
				UClass* EffectClass = Definition.GrantedEffects[Index].LoadSynchronous();
				if (!EffectClass)
				{
					UE_LOG(LogAugmentAssembler, Error,
						TEXT("[AugmentAssembler] %s 의 GrantedEffects[%d] 를 로드하지 못했습니다"),
						*Definition.GetName(), Index);
					continue;
				}

				FGameplayEffectContextHandle Context = ASC.MakeEffectContext();

				// 어느 증강이 건 효과인지
				Context.AddSourceObject(&Definition);

				// GE 레벨은 항상 1
				const FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(EffectClass, 1.f, Context);
				if (!Spec.IsValid())
				{
					UE_LOG(LogAugmentAssembler, Error,
						TEXT("[AugmentAssembler] %s 의 GrantedEffects[%d] 스펙 생성에 실패했습니다"),
						*Definition.GetName(), Index);
					continue;
				}

				// 각 레벨의 SetByCallerMagnitude 를 스펙에 적용. Magnitude 는 레벨마다 다르게 설정
				for (const TPair<FGameplayTag, float>& Magnitude : LevelData.SetByCallerMagnitudes)
				{
					Spec.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
				}

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

				OutHandles.EffectHandles.Add(Handle);
			}
		}

		void GiveGrantedAbilities(UAbilitySystemComponent& ASC,
		                          const UAugmentDefinition& Definition,
		                          int32 Level,
		                          FAugmentGrantHandles& OutHandles)
		{
			for (int32 Index = 0; Index < Definition.GrantedAbilities.Num(); ++Index)
			{
				const FAugmentGrantedAbility& Granted = Definition.GrantedAbilities[Index];

				UClass* AbilityClass = Granted.AbilityClass.LoadSynchronous();
				if (!AbilityClass)
				{
					UE_LOG(LogAugmentAssembler, Error,
						TEXT("[AugmentAssembler] %s 의 GrantedAbilities[%d] 를 로드하지 못했습니다"),
						*Definition.GetName(), Index);
					continue;
				}

				// SourceObject 로 정의를 전달
				// GAS 가 non-const 를 요구해서 const_cast
				UObject* SourceObject = const_cast<UAugmentDefinition*>(&Definition);

				// 어빌리티 레벨 = 증강 레벨
				FGameplayAbilitySpec Spec(AbilityClass, Level, INDEX_NONE, SourceObject);

				if (Granted.InputTag.IsValid())
				{
					// 입력 태그 부여
					Spec.GetDynamicSpecSourceTags().AddTag(Granted.InputTag);
				}

				const FGameplayAbilitySpecHandle Handle = ASC.GiveAbility(Spec);
				if (!Handle.IsValid())
				{
					UE_LOG(LogAugmentAssembler, Error,
						TEXT("[AugmentAssembler] %s 의 GrantedAbilities[%d] 부여에 실패했습니다"),
						*Definition.GetName(), Index);
					continue;
				}

				OutHandles.AbilityHandles.Add(Handle);
			}
		}

		bool CanLoadAllAssets(const UAugmentDefinition& Definition)
		{
			bool bLoadable = true;

			for (int32 Index = 0; Index < Definition.GrantedEffects.Num(); ++Index)
			{
				if (!Definition.GrantedEffects[Index].LoadSynchronous())
				{
					UE_LOG(LogAugmentAssembler, Error,
						TEXT("[AugmentAssembler] %s 의 GrantedEffects[%d] 를 로드하지 못했습니다"),
						*Definition.GetName(), Index);
					bLoadable = false;
				}
			}

			for (int32 Index = 0; Index < Definition.GrantedAbilities.Num(); ++Index)
			{
				if (!Definition.GrantedAbilities[Index].AbilityClass.LoadSynchronous())
				{
					UE_LOG(LogAugmentAssembler, Error,
						TEXT("[AugmentAssembler] %s 의 GrantedAbilities[%d] 를 로드하지 못했습니다"),
						*Definition.GetName(), Index);
					bLoadable = false;
				}
			}

			return bLoadable;
		}
	}
}

bool RogueAugmentAssembler::CanApplyAugment(const UAugmentDefinition& Definition, int32 Level)
{
	if (!Definition.FindLevel(Level))
	{
		UE_LOG(LogAugmentAssembler, Error,
			TEXT("[AugmentAssembler] %s 에 %d레벨이 없습니다 (최고 %d레벨)"),
			*Definition.GetName(), Level, Definition.GetMaxLevel());
		return false;
	}

	return CanLoadAllAssets(Definition);
}

FAugmentGrantHandles RogueAugmentAssembler::ApplyAugment(UAbilitySystemComponent& ASC,
                                                         const UAugmentDefinition& Definition,
                                                         int32 Level)
{
	FAugmentGrantHandles Handles;

	// 증강 부여는 서버에서
	if (!ASC.IsOwnerActorAuthoritative())
	{
		UE_LOG(LogAugmentAssembler, Error,
			TEXT("[AugmentAssembler] 권한이 없는 곳에서 %s 를 부여하려 했습니다"), *Definition.GetName());
		return Handles;
	}

	const FAugmentLevelData* LevelData = Definition.FindLevel(Level);
	if (!LevelData)
	{
		UE_LOG(LogAugmentAssembler, Error,
			TEXT("[AugmentAssembler] %s 에 %d레벨이 없습니다 (최고 %d레벨)"),
			*Definition.GetName(), Level, Definition.GetMaxLevel());
		return Handles;
	}

	ApplyGrantedEffects(ASC, Definition, *LevelData, Handles);
	GiveGrantedAbilities(ASC, Definition, Level, Handles);

	return Handles;
}

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
