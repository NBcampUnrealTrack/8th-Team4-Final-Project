// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueAbilitySystemComponent.h"

#include "GAS/RogueAbilitySet.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogRogueAbility, Log, Log);

namespace
{
	FPredictionKey GetInstanceActivationPredictionKey(const FGameplayAbilitySpec& Spec)
	{
		const TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();

		return Instances.IsEmpty()
			? FPredictionKey()
			: Instances.Last()->GetCurrentActivationInfo().GetActivationPredictionKey();
	}
}

URogueAbilitySystemComponent::URogueAbilitySystemComponent()
{
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;

	SetIsReplicatedByDefault(true);
}

void URogueAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid() || HasMatchingGameplayTag(RogueTags::State_Gameplay_InputBlocked))
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> ToActivate;
	int32 MatchedSpecCount = 0;

	{
		ABILITYLIST_SCOPE_LOCK();

		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				continue;
			}

			++MatchedSpecCount;

			if (Spec.IsActive())
			{
				// AbilitySpecInputPressed 는 플래그만 세우고 어빌리티 인스턴스에 전달
				// 태그 경로에서는 여기서 직접 보냄
				AbilitySpecInputPressed(Spec);
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed,
					Spec.Handle, GetInstanceActivationPredictionKey(Spec));
			}
			else
			{
				// 아직 안 켜졌으면 눌림 플래그 활성화
				Spec.InputPressed = true;
				ToActivate.Add(Spec.Handle);
			}
		}
	}

	int32 ActivatedCount = 0;
	for (const FGameplayAbilitySpecHandle& Handle : ToActivate)
	{
		if (TryActivateAbility(Handle))
		{
			++ActivatedCount;
			break;
		}
	}

	ReportInputResult(InputTag, MatchedSpecCount, ToActivate.Num(), ActivatedCount);
}

void URogueAbilitySystemComponent::ReportInputResult(
	const FGameplayTag& InputTag, int32 MatchedSpecCount, int32 CandidateCount, int32 ActivatedCount)
{
	// 이 태그를 가진 어빌리티가 아예 없는 경우
	if (MatchedSpecCount == 0)
	{
		UE_LOG(LogRogueAbility, Error,
			TEXT("[RogueAbility] 입력 태그 %s 를 가진 어빌리티가 없습니다. "
			     "프로젝트 설정 > 게임 > Rogue GAS > Startup Ability Set 및 "
			     "해당 AbilitySet의 InputTag를 확인하세요"),
			*InputTag.ToString());
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	FInputDiagnostic& Diagnostic = InputDiagnostics.FindOrAdd(InputTag);

	if (ActivatedCount > 0)
	{
		Diagnostic.LastActivatedTime = Now;
		return;
	}

	// 후보가 없으면 매칭된 스펙이 전부 이미 활성 상태라는 뜻이라 정상이다
	if (CandidateCount == 0)
	{
		return;
	}

	constexpr double RecentActivationWindow = 1.0;   
	constexpr double RejectLogInterval      = 1.0;

	const bool bRecentlyActivated = Diagnostic.LastActivatedTime > 0.0
		&& (Now - Diagnostic.LastActivatedTime) < RecentActivationWindow;
	const bool bLogIntervalPassed = Diagnostic.LastLogTime <= 0.0
		|| (Now - Diagnostic.LastLogTime) >= RejectLogInterval;

	if (bRecentlyActivated || !bLogIntervalPassed)
	{
		return;
	}

	Diagnostic.LastLogTime = Now;

	UE_LOG(LogRogueAbility, Log,
		TEXT("[RogueAbility] %s: 후보 %d개가 전부 활성화를 거절했습니다. "
		     "CanActivateAbility 조건과 차단 태그/쿨다운/코스트를 확인하세요"),
		*InputTag.ToString(), CandidateCount);
}

void URogueAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();

	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		Spec.InputPressed = false;

		if (!Spec.IsActive())
		{
			continue;
		}

		AbilitySpecInputReleased(Spec);

		// WaitInputRelease 태스크는 이 복제 이벤트를 기다림
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased,
			Spec.Handle, GetInstanceActivationPredictionKey(Spec));
	}
}

void URogueAbilitySystemComponent::SetEquippedWeaponType(FGameplayTag EquippedStateTag)
{
	if (EquippedStateTag == CurrentEquippedWeaponTag)
	{
		return;
	}

	// AddLoose/RemoveLoose 가 아니라 개수를 직접 세팅
	if (CurrentEquippedWeaponTag.IsValid())
	{
		SetLooseGameplayTagCount(CurrentEquippedWeaponTag, 0, EGameplayTagReplicationState::TagOnly);
	}

	CurrentEquippedWeaponTag = EquippedStateTag;

	if (EquippedStateTag.IsValid())
	{
		SetLooseGameplayTagCount(EquippedStateTag, 1, EGameplayTagReplicationState::TagOnly);
	}

	UE_LOG(LogRogueAbility, Log,
		TEXT("[RogueAbility] 장착 태그 -> %s"),
		EquippedStateTag.IsValid() ? *EquippedStateTag.ToString() : TEXT("(없음 - 맨몸)"));
}

void URogueAbilitySystemComponent::GrantAbilitySet(const URogueAbilitySet* AbilitySet)
{
	if (!IsOwnerActorAuthoritative())
	{
		UE_LOG(LogRogueAbility, Error,
			TEXT("[RogueAbility] 권한이 없는 곳에서 어빌리티를 부여하려 했습니다. "
			     "서버에서 주면 스펙이 소유 클라이언트로 복제됩니다"));
		return;
	}

	if (!AbilitySet)
	{
		return;
	}

	for (int32 Index = 0; Index < AbilitySet->Abilities.Num(); ++Index)
	{
		const FRogueGrantedAbility& Granted = AbilitySet->Abilities[Index];

		if (!Granted.AbilityClass)
		{
			UE_LOG(LogRogueAbility, Error,
				TEXT("[RogueAbility] %s 의 Abilities[%d] 가 비어 있습니다"), *AbilitySet->GetName(), Index);
			continue;
		}

		// 나중에 아바타 교체 정리 시 AbilitySet 을 참조해 부여한 어빌리티를 찾아 제거
		FGameplayAbilitySpec Spec(Granted.AbilityClass, 1, INDEX_NONE, const_cast<URogueAbilitySet*>(AbilitySet));

		if (Granted.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Granted.InputTag);
		}

		GiveAbility(Spec);
	}

	UE_LOG(LogRogueAbility, Log,
		TEXT("[RogueAbility] %s 어빌리티 %d개 부여"), *AbilitySet->GetName(), AbilitySet->Abilities.Num());
}

int32 URogueAbilitySystemComponent::RemoveRoundScopedEffects()
{
	if (!IsOwnerActorAuthoritative())
	{
		UE_LOG(LogRogueAbility, Error,
			TEXT("[RogueAbility] 권한이 없는 곳에서 라운드 한정 효과를 제거하려 했습니다. "
			     "서버에서 제거하면 클라이언트로 복제됩니다"));
		return 0;
	}

	// 라운드 한정 효과 태그를 가진 모든 GE 제거
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(
		FGameplayTagContainer(RogueTags::Effect_Lifetime_Round));

	const int32 RemovedCount = RemoveActiveEffects(Query);
	if (RemovedCount > 0)
	{
		UE_LOG(LogRogueAbility, Log,
			TEXT("[RogueAbility] %s: 라운드 한정 효과 %d 개 제거"),
			*GetNameSafe(GetOwnerActor()), RemovedCount);
	}

	return RemovedCount;
}
