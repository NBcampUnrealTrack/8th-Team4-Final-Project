// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/RogueGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "Tags/GameplayTags.h"

URogueTimedTagEffect::URogueTimedTagEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataTag = RogueTags::Data_Magnitude_Duration;

	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
}

int32 URogueGameplayAbility::GetMaxCharges(int32 Level) const
{
	// 문맥 문자열을 넘기면 커브 행을 못 찾을 때 경고
	static const FString Context(TEXT("URogueGameplayAbility::MaxCharges"));

	// 0 이나 음수면 어빌리티가 잠김
	return FMath::Max(1, MaxCharges.AsInteger(Level, &Context));
}

bool URogueGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo,
                                          FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!CooldownTag.IsValid())
	{
		return true;
	}

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return true;
	}

	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(CooldownTag);

	// 현재 스택이 최대 충전 수보다 적으면 쿨타임이 남아있어도 사용 가능
	const int32 SpentCharges =
		ASC->GetAggregatedStackCount(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags));

	if (SpentCharges < GetMaxCharges(GetAbilityLevel(Handle, ActorInfo)))
	{
		return true;
	}

	// 실패 태그 및 쿨타임 태그 반환
	if (OptionalRelevantTags)
	{
		const FGameplayTag& FailCooldownTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;
		if (FailCooldownTag.IsValid())
		{
			OptionalRelevantTags->AddTag(FailCooldownTag);
		}

		OptionalRelevantTags->AppendMatchingTags(ASC->GetOwnedGameplayTags(), CooldownTags);
	}

	return false;
}

const FGameplayTagContainer* URogueGameplayAbility::GetCooldownTags() const
{
	CooldownTagContainer.Reset();

	if (CooldownTag.IsValid())
	{
		CooldownTagContainer.AddTag(CooldownTag);
	}

	return &CooldownTagContainer;
}

void URogueGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 태그가 없으면 쿨타임이 없는 어빌리티
	if (!CooldownTag.IsValid())
	{
		return;
	}

	const int32 Level = GetAbilityLevel(Handle, ActorInfo);

	static const FString Context(TEXT("URogueGameplayAbility::BaseCooldown"));
	const float LevelCooldown = BaseCooldown.GetValueAtLevel(Level, &Context);

	// 쿨타임이 0초면 쿨타임이 없는 어빌리티
	if (LevelCooldown <= 0.f)
	{
		return;
	}

	// 어트리뷰트는 ASC 소유자(PlayerState)에 존재
	const AActor* StatSource = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr;

	// 쿨타임이 0 이하면 0초로 처리
	float Duration = URogueAugmentStatResolver::GetFinalCooldown(StatSource, LevelCooldown);
	if (Duration <= 0.f)
	{
		return;
	}

	// 이전 쿨타임이 남아있으면 그만큼 더해, 
	// 두 번째 사용의 쿨타임이 첫 번째 사용의 쿨타임이 끝나면 적용
	Duration += FMath::Max(0.f, GetCooldownTimeRemaining(ActorInfo));

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo,
		URogueTimedTagEffect::StaticClass(), Level);

	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RogueTags::Data_Magnitude_Duration, Duration);

	// 어빌리티별 쿨타임 태그
	SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownTag);

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
