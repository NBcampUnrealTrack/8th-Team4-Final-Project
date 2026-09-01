// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Augment/Abilities/RogueGameplayAbility.h"
#include "RogueSpawnActorAbility.generated.h"

// 페이로드를 액터에 넘기고 끝나는 증강의 공통 부모
// 증강의 실행 효과는 스폰된 액터가 담당
UCLASS(Abstract)
class FPS_API URogueSpawnActorAbility : public URogueGameplayAbility
{
	GENERATED_BODY()

public:
	URogueSpawnActorAbility();

protected:
	virtual void Deliver(const FAugmentGrantedAbility& Entry,
	                     const FRogueEffectPayload& Payload,
	                     FGameplayAbilitySpecHandle Handle,
	                     const FGameplayAbilityActorInfo* ActorInfo,
	                     const FGameplayAbilityActivationInfo ActivationInfo,
	                     const FGameplayEventData* TriggerEventData) override;

	// 시전자의 전방으로 SpawnForwardOffset 만큼 떨어진 위치에 스폰
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Spawn", meta = (ClampMin = "0.0"))
	float SpawnForwardOffset = 100.f;
};
