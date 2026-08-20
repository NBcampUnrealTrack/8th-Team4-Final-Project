// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Abilities/RogueGameplayAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "GAS/RogueGASSettings.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentAbility, Log, All);

namespace
{
	// 설정에 등록된 GE 애셋을 로드 (쿨타임/지속 시간)
	UClass* LoadConfiguredEffectClass(const TSoftClassPtr<UGameplayEffect>& SoftClass, const TCHAR* FieldName)
	{
		UClass* Loaded = SoftClass.LoadSynchronous();
		if (!Loaded)
		{
			UE_LOG(LogAugmentAbility, Error,
				TEXT("[AugmentAbility] %s 를 로드하지 못했습니다. DefaultGame.ini 의 [/Script/FPS.RogueGASSettings] 등록과 ")
				TEXT("애셋 경로를 확인하세요 (Duration Policy = Has Duration, Duration = SetByCaller %s)"),
				FieldName, *RogueTags::Data_Magnitude_Duration.GetTag().ToString());
		}

		return Loaded;
	}
}

UClass* URogueGameplayAbility::GetCooldownEffectClass()
{
	return LoadConfiguredEffectClass(GetDefault<URogueGASSettings>()->CooldownEffectClass, TEXT("CooldownEffectClass"));
}

UClass* URogueGameplayAbility::GetStatusEffectClass()
{
	return LoadConfiguredEffectClass(GetDefault<URogueGASSettings>()->StatusEffectClass, TEXT("StatusEffectClass"));
}

void URogueGameplayAbility::CommitExecute(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::CommitExecute(Handle, ActorInfo, ActivationInfo);

	if (!bBroadcastSkillUsed)
	{
		return;
	}

	// 이벤트 발신은 서버에서만
	if (!HasAuthority(&ActivationInfo))
	{
		return;
	}

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag   = RogueTags::Event_Skill_Used;
	Payload.Instigator = Avatar;
	Payload.Target     = Avatar;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, RogueTags::Event_Skill_Used, Payload);
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

	const FGameplayTagContainer& CooldownTags = *GetCooldownTags();

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

	UClass* CooldownEffectClass = GetCooldownEffectClass();
	if (!CooldownEffectClass)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo,
		CooldownEffectClass, Level);

	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RogueTags::Data_Magnitude_Duration, Duration);

	// 어빌리티별 쿨타임 태그
	SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownTag);

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
