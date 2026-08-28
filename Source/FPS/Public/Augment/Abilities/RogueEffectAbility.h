// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Augment/Abilities/RogueGameplayAbility.h"
#include "RogueEffectAbility.generated.h"

// 조립된 페이로드의 대상 전원에게 GE 를 거는 증강의 공통 부모
//   이벤트 : NetExecutionPolicy = ServerOnly, AbilityTriggers 지정
//   입력   : NetExecutionPolicy = LocalPredicted, bBroadcastSkillUsed = true
UCLASS(Abstract)
class FPS_API URogueEffectAbility : public URogueGameplayAbility
{
	GENERATED_BODY()

public:
	URogueEffectAbility();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void Deliver(const FRogueEffectPayload& Payload,
	                     FGameplayAbilitySpecHandle Handle,
	                     const FGameplayAbilityActorInfo* ActorInfo,
	                     const FGameplayAbilityActivationInfo ActivationInfo,
	                     const FGameplayEventData* TriggerEventData) override;
};
