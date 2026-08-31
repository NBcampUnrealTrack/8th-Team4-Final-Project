// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Abilities/RogueEffectAbility.h"

#include "GAS/RogueEffectApplier.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RogueEffectAbility"

URogueEffectAbility::URogueEffectAbility()
{
	// 전투 이벤트는 서버에서만 발신되므로 예측 X. 입력 발동 증강은 BP 에서 LocalPredicted 로 직접 변경
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URogueEffectAbility::Deliver(const FAugmentGrantedAbility& Entry,
                                  const FRogueEffectPayload& Payload,
                                  const FGameplayAbilitySpecHandle Handle,
                                  const FGameplayAbilityActorInfo* ActorInfo,
                                  const FGameplayAbilityActivationInfo ActivationInfo,
                                  const FGameplayEventData* TriggerEventData)
{
	URogueEffectApplier::Apply(Payload);
}

#if WITH_EDITOR
EDataValidationResult URogueEffectAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 액티브 스킬도 아니고, 트리거도 없으면 에러
	if (!bBroadcastSkillUsed && AbilityTriggers.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoActivationPath",
			"Ability Triggers 가 비어 있습니다. 이벤트로 발동하면 트리거를 지정하고, "
			"입력으로 발동하면 bBroadcastSkillUsed 를 켜세요"));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
