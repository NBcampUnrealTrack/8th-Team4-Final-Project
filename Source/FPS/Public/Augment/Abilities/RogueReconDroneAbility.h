// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Augment/Abilities/RogueGameplayAbility.h"
#include "RogueReconDroneAbility.generated.h"

UCLASS()
class FPS_API URogueReconDroneAbility : public URogueGameplayAbility
{
	GENERATED_BODY()

public:
	URogueReconDroneAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	// 탐지 거리(cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Recon")
	FScalableFloat ScanRange = FScalableFloat(1500.f);

	// 탐지 굵기(cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Recon")
	FScalableFloat ScanRadius = FScalableFloat(1000.f);

	// 발각 지속시간(초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Recon")
	FScalableFloat RevealDuration = FScalableFloat(3.f);

	// 한 번에 발각시킬 최대 인원
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Recon")
	FScalableFloat MaxTargets = FScalableFloat(1.f);

private:
	void ApplyReveal(const FGameplayAbilitySpecHandle Handle,
	                 const FGameplayAbilityActorInfo* ActorInfo,
	                 const FGameplayAbilityActivationInfo ActivationInfo,
	                 AActor* Target, float Duration, int32 Level) const;
};
