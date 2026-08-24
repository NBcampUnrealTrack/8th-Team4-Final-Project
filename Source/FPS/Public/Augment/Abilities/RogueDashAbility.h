// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Augment/Abilities/RogueGameplayAbility.h"
#include "RogueDashAbility.generated.h"

class ACharacter;

UCLASS()
class FPS_API URogueDashAbility : public URogueGameplayAbility
{
	GENERATED_BODY()

public:
	URogueDashAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	// 이동 거리(cm). 레벨 스케일은 Curve
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Dash")
	FScalableFloat DashDistance = FScalableFloat(500.f);

	// 대시 시간(초). 레벨 스케일은 Curve
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Dash")
	FScalableFloat DashDuration = FScalableFloat(0.15f);

private:
	// 입력이 없으면 정면
	FVector ResolveDashDirection(const ACharacter& Character) const;

	UFUNCTION()
	void HandleDashFinished();
};
