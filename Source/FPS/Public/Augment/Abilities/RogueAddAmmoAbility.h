// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Augment/Abilities/RogueGameplayAbility.h"

#include "RogueAddAmmoAbility.generated.h"

UCLASS()
class FPS_API URogueAddAmmoAbility : public URogueGameplayAbility
{
	GENERATED_BODY()

public:
	URogueAddAmmoAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	// 반복형은 소유 클라가 시작
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
	                        const FGameplayAbilityActorInfo* ActorInfo,
	                        const FGameplayAbilityActivationInfo ActivationInfo,
	                        bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	// 실제 바뀐 양 반환
	int32 AddAmmo();

	// 서버가 예측을 거절하면 클라에서 더한 만큼 되돌림
	void HandleAddRejected(int32 Added);

	UFUNCTION()
	void HandleRepeatFinished();

	FTimerHandle AddAmmoTimer;
};
