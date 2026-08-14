// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/RogueGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "GAS/RogueGASSettings.h"
#include "GameFlow/Round/BaseRoundPlayerController.h"
#include "GameFramework/Pawn.h"
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

bool URogueGameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 입력 매핑 제거만으로 막지 않고 서버의 어빌리티 활성화 검증에도 라운드 상태를 반영한다.
	// 현재 이 베이스의 실사용자는 Dash/Recon이라 라운드 전/선택/종료/사망/폭탄 상호작용 중에는
	// 액티브 증강도 무기 어빌리티와 동일하게 차단된다.
	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const ABaseRoundPlayerController* RoundPlayerController = IsValid(AvatarPawn)
		? Cast<ABaseRoundPlayerController>(AvatarPawn->GetController())
		: nullptr;

	return !IsValid(RoundPlayerController) || RoundPlayerController->CanUseCombatInput();
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
