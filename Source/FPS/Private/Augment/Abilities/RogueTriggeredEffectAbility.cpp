// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Abilities/RogueTriggeredEffectAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Augment/AugmentDefinition.h"
#include "Combat/CombatComponent.h"
#include "GAS/Damage/RogueGameplayEffectContext.h"
#include "GAS/RogueTargetFilter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RogueTriggeredEffectAbility"

DEFINE_LOG_CATEGORY_STATIC(LogTriggeredAugment, Log, All);

URogueTriggeredEffectAbility::URogueTriggeredEffectAbility()
{
	// 전투 이벤트는 서버에서만 발신되므로 예측 X
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 배타가 필요한 증강은 자식에서 명시
}

void URogueTriggeredEffectAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                   const FGameplayAbilityActorInfo* ActorInfo,
                                                   const FGameplayAbilityActivationInfo ActivationInfo,
                                                   const FGameplayEventData* TriggerEventData)
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (!Avatar || !SourceASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// PDA의 GrantedAbilities 항목에서 해당 GA가 없는 경우
	const FAugmentGrantedAbility* Entry = FindGrantedEntry(Handle, ActorInfo);
	if (!Entry)
	{
		UE_LOG(LogTriggeredAugment, Error,
			TEXT("[TriggeredAugment] %s 를 부여한 증강 정의에서 자기 GrantedAbilities 항목을 찾지 못했습니다. ")
			TEXT("PDA 의 AbilityClass 가 이 클래스를 가리키는지 확인하세요"), *GetName());

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 무기 조건이 안 맞는 이벤트는 발동 X
	if (!Entry->RequiredWeaponTags.IsEmpty())
	{
		const FGameplayTag EventWeaponTag = ResolveEventWeaponTag(Avatar, TriggerEventData);

		if (!EventWeaponTag.MatchesAny(Entry->RequiredWeaponTags))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}
	}

	// 이벤트 대상이 없으면 발동 X
	AActor* TargetActor = ResolveTarget(Avatar, TriggerEventData, *Entry);
	if (!TargetActor)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 발동할 GE가 없으면 발동 X
	if (Entry->EffectsToApply.IsEmpty())
	{
		UE_LOG(LogTriggeredAugment, Error,
			TEXT("[TriggeredAugment] %s 를 부여한 계열의 EffectsToApply 가 비어 있습니다"), *GetName());

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// GE 를 먼저 전부 로드해 두고, 하나라도 실패하면 발동 X
	TArray<UClass*> EffectClasses;
	EffectClasses.Reserve(Entry->EffectsToApply.Num());

	for (int32 Index = 0; Index < Entry->EffectsToApply.Num(); ++Index)
	{
		UClass* EffectClass = Entry->EffectsToApply[Index].LoadSynchronous();
		if (!EffectClass)
		{
			UE_LOG(LogTriggeredAugment, Error,
				TEXT("[TriggeredAugment] %s 의 EffectsToApply[%d] 를 로드하지 못했습니다"),
				*GetName(), Index);

			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		EffectClasses.Add(EffectClass);
	}

	// 실제로 효과를 걸기 직전에만 쿨타임/코스트를 소모
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const int32 Level = GetAbilityLevel(Handle, ActorInfo);

	// 증강 레벨에 따라 SetByCaller 수치 적용 후 대상에게 GE 적용
	for (UClass* EffectClass : EffectClasses)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
			Handle, ActorInfo, ActivationInfo, EffectClass, Level);

		if (!SpecHandle.IsValid())
		{
			UE_LOG(LogTriggeredAugment, Error,
				TEXT("[TriggeredAugment] %s 의 %s 스펙 생성에 실패했습니다"),
				*GetName(), *GetNameSafe(EffectClass));
			continue;
		}

		InjectLevelMagnitudes(SpecHandle, Handle, ActorInfo);

		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// 이 어빌리티를 부여한 계열의 GrantedAbilities 항목
const FAugmentGrantedAbility* URogueTriggeredEffectAbility::FindGrantedEntry(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAugmentDefinition* Definition = Cast<UAugmentDefinition>(GetSourceObject(Handle, ActorInfo));
	if (!Definition)
	{
		return nullptr;
	}

	const FSoftObjectPath SelfPath(GetClass());

	return Definition->GrantedAbilities.FindByPredicate(
		[&SelfPath](const FAugmentGrantedAbility& Granted)
		{
			return Granted.AbilityClass.ToSoftObjectPath() == SelfPath;
		});
}

FGameplayTag URogueTriggeredEffectAbility::ResolveEventWeaponTag(
	AActor* Avatar, const FGameplayEventData* TriggerEventData) const
{
	// 대미지 이벤트는 컨텍스트의 무기 태그 사용
	if (TriggerEventData)
	{
		const FRogueGameplayEffectContext* Context =
			FRogueGameplayEffectContext::ExtractFrom(TriggerEventData->ContextHandle);

		if (Context && Context->WeaponTag.IsValid())
		{
			return Context->WeaponTag;
		}
	}

	// 사격/재장전/교체 이벤트는 현재 장착 무기 태그 사용
	const UCombatComponent* Combat = IsValid(Avatar)
		? Avatar->FindComponentByClass<UCombatComponent>()
		: nullptr;

	return Combat ? Combat->GetCurrentWeaponTag() : FGameplayTag();
}

// 효과를 받을 액터 결정
AActor* URogueTriggeredEffectAbility::ResolveTarget(AActor* Avatar,
                                                    const FGameplayEventData* TriggerEventData,
                                                    const FAugmentGrantedAbility& Entry) const
{
	if (!Entry.bApplyToEventTarget)
	{
		return Avatar;
	}

	if (!TriggerEventData)
	{
		return nullptr;
	}

	AActor* TargetActor = const_cast<AActor*>(TriggerEventData->Target.Get());
	if (!TargetActor || !URogueTargetFilter::IsAlive(TargetActor))
	{
		return nullptr;
	}

	if (Entry.bHostileTargetOnly
		&& URogueTargetFilter::GetRelation(Avatar, TargetActor) != ERogueTargetRelation::Hostile)
	{
		return nullptr;
	}

	return TargetActor;
}

// 증강 레벨에 따라 SetByCaller 수치 적용
void URogueTriggeredEffectAbility::InjectLevelMagnitudes(const FGameplayEffectSpecHandle& SpecHandle,
                                                          const FGameplayAbilitySpecHandle Handle,
                                                          const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAugmentDefinition* Definition = Cast<UAugmentDefinition>(GetSourceObject(Handle, ActorInfo));
	if (!Definition)
	{
		return;
	}

	const FAugmentLevelData* LevelData = Definition->FindLevel(GetAbilityLevel(Handle, ActorInfo));
	if (!LevelData)
	{
		return;
	}

	for (const TPair<FGameplayTag, float>& Magnitude : LevelData->SetByCallerMagnitudes)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
	}
}

#if WITH_EDITOR
EDataValidationResult URogueTriggeredEffectAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 트리거 태그가 없는 경우
	if (AbilityTriggers.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoTriggers",
			"Ability Triggers 가 비어 있습니다. Gameplay Event 트리거를 지정하세요"));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
