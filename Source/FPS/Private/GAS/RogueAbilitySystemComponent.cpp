// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueAbilitySystemComponent.h"
#include "Abilities/GA_ADS.h"

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

void URogueAbilitySystemComponent::AbilityInputTagPressed(
	const FGameplayTag& InputTag, const bool bToggleADS, const bool bIsNewPress)
{
	if (!ensure(InputTag.IsValid()))
	{
		return;
	}

	if (HasMatchingGameplayTag(RogueTags::State_Gameplay_InputBlocked))
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> ToActivate;
	int32 MatchedSpecCount = 0;

	{
		ABILITYLIST_SCOPE_LOCK();

		// 같은 입력에 ADS 스펙이 여러 개여도 해제 클릭으로 다른 ADS를 켜지 않는다.
		bool bToggleOffADS = false;
		if (bToggleADS && bIsNewPress)
		{
			for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
			{
				if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)
					&& Spec.Ability && Spec.Ability->IsA<UGA_ADS>() && Spec.IsActive())
				{
					bToggleOffADS = true;
					break;
				}
			}
		}

		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				continue;
			}

			++MatchedSpecCount;

			// 같은 입력 태그를 쓰는 다른 어빌리티의 홀드 동작은 그대로 둔다.
			if (bToggleADS && Spec.Ability && Spec.Ability->IsA<UGA_ADS>())
			{
				if (!bIsNewPress)
				{
					continue;
				}
				if (bToggleOffADS)
				{
					ReleaseAbilityInput(Spec);
					continue;
				}
			}

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

void URogueAbilitySystemComponent::AbilityInputTagReleased(
	const FGameplayTag& InputTag, const bool bPreserveADS)
{
	if (!ensure(InputTag.IsValid()))
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

		if (bPreserveADS && Spec.Ability && Spec.Ability->IsA<UGA_ADS>())
		{
			continue;
		}

		ReleaseAbilityInput(Spec);
	}
}

void URogueAbilitySystemComponent::ReleaseADSInput()
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.Ability->IsA<UGA_ADS>())
		{
			continue;
		}
		ReleaseAbilityInput(Spec);
	}
}

void URogueAbilitySystemComponent::ReleaseAbilityInput(FGameplayAbilitySpec& Spec)
{
	// 호출부가 어빌리티 목록을 잠근 상태에서 입력 해제를 처리한다.
	Spec.InputPressed = false;
	if (!Spec.IsActive())
	{
		return;
	}

	AbilitySpecInputReleased(Spec);
	// WaitInputRelease 태스크는 이 복제 이벤트를 기다린다.
	InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased,
		Spec.Handle, GetInstanceActivationPredictionKey(Spec));
}

void URogueAbilitySystemComponent::CancelADSAbilityForWeapon(const AWeaponBase* Weapon)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (!Spec.Ability || !Spec.Ability->IsA<UGA_ADS>())
			{
				continue;
			}
			bool bMatchesWeapon = !Spec.IsActive();
			for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
			{
				const UGA_ADS* ADS = Cast<UGA_ADS>(Instance);
				bMatchesWeapon |= ADS && ADS->IsAimingWithWeapon(Weapon);
			}
			if (bMatchesWeapon)
			{
				Spec.InputPressed = false;
				if (Spec.IsActive())
				{
					Handles.Add(Spec.Handle);
				}
			}
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		CancelAbilityHandle(Handle);
	}
}

void URogueAbilitySystemComponent::ClearAbilityInput()
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		Spec.InputPressed = false;
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
	if (!ensureMsgf(IsOwnerActorAuthoritative(),
		TEXT("[RogueAbility] 권한이 없는 곳에서 어빌리티를 부여하려 했습니다. "
		     "서버에서 주면 스펙이 소유 클라이언트로 복제됩니다")))
	{
		return;
	}

	if (!ensure(AbilitySet))
	{
		return;
	}

	for (int32 Index = 0; Index < AbilitySet->Abilities.Num(); ++Index)
	{
		const FRogueGrantedAbility& Granted = AbilitySet->Abilities[Index];

		if (!ensureMsgf(Granted.AbilityClass,
			TEXT("[RogueAbility] %s 의 Abilities[%d] 가 비어 있습니다"), *AbilitySet->GetName(), Index))
		{
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

	for (const TSubclassOf<UGameplayEffect>& EffectClass : AbilitySet->Effects)
	{
		if (ensureMsgf(EffectClass, TEXT("[RogueAbility] %s 의 Effects 에 빈 항목이 있습니다"), *AbilitySet->GetName()))
		{
			ApplyGameplayEffectToSelf(EffectClass.GetDefaultObject(), 1.f, MakeEffectContext());
		}
	}

	UE_LOG(LogRogueAbility, Log,
		TEXT("[RogueAbility] %s 어빌리티 %d개 부여"), *AbilitySet->GetName(), AbilitySet->Abilities.Num());
}

int32 URogueAbilitySystemComponent::RemoveRoundScopedEffects()
{
	if (!ensureMsgf(IsOwnerActorAuthoritative(),
		TEXT("[RogueAbility] 권한이 없는 곳에서 라운드 한정 효과를 제거하려 했습니다. "
		     "서버에서 제거하면 클라이언트로 복제됩니다")))
	{
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

void URogueAbilitySystemComponent::ReduceActiveCooldowns(float Rate)
{
	const UWorld* World = GetWorld();
	if (Rate <= 0.f || !World || !IsOwnerActorAuthoritative())
	{
		return;
	}

	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(RogueTags::Cooldown_Augment_Slot1);
	CooldownTags.AddTag(RogueTags::Cooldown_Augment_Slot2);
	CooldownTags.AddTag(RogueTags::Cooldown_Augment_Slot3);

	const float Now = World->GetTimeSeconds();
	for (const FActiveGameplayEffectHandle& Handle : GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags)))
	{
		if (const FActiveGameplayEffect* Active = GetActiveGameplayEffect(Handle))
		{
			ModifyActiveEffectStartTime(Handle, -Active->GetTimeRemaining(Now) * FMath::Min(Rate, 1.f));
		}
	}
}

void URogueAbilitySystemComponent::OnGameplayEffectDurationChange(FActiveGameplayEffect& ActiveEffect)
{
	Super::OnGameplayEffectDurationChange(ActiveEffect);

	FGameplayTagContainer GrantedTags;
	ActiveEffect.Spec.GetAllGrantedTags(GrantedTags);

	for (const FGameplayTag& SlotTag : { RogueTags::Cooldown_Augment_Slot1.GetTag(),
		RogueTags::Cooldown_Augment_Slot2.GetTag(), RogueTags::Cooldown_Augment_Slot3.GetTag() })
	{
		if (GrantedTags.HasTagExact(SlotTag))
		{
			OnCooldownTimeChanged.Broadcast(SlotTag);
		}
	}
}
