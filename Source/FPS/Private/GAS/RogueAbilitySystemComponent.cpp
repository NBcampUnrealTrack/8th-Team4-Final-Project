// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueAbilitySystemComponent.h"

#include "GAS/RogueAbilitySet.h"

DEFINE_LOG_CATEGORY_STATIC(LogRogueAbility, Log, All);

namespace
{
	FPredictionKey GetInstanceActivationPredictionKey(const FGameplayAbilitySpec& Spec)
	{
		const TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();

		return Instances.IsEmpty()
			? Spec.ActivationInfo.GetActivationPredictionKey()
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
	if (!InputTag.IsValid())
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> ToActivate;

	{
		ABILITYLIST_SCOPE_LOCK();

		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				continue;
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

	for (const FGameplayAbilitySpecHandle& Handle : ToActivate)
	{
		TryActivateAbility(Handle);
	}
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
