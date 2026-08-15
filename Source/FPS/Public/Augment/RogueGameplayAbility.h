// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "ScalableFloat.h"
#include "RogueGameplayAbility.generated.h"

UCLASS(HideDropdown)
class URogueTimedTagEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URogueTimedTagEffect();
};


UCLASS(Abstract)
class FPS_API URogueGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URogueGameplayAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                                const FGameplayAbilityActorInfo* ActorInfo,
	                                const FGameplayTagContainer* SourceTags = nullptr,
	                                const FGameplayTagContainer* TargetTags = nullptr,
	                                FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	                           const FGameplayAbilityActorInfo* ActorInfo,
	                           const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// 충전이 남았으면 통과
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle,
	                           const FGameplayAbilityActorInfo* ActorInfo,
	                           OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	// 어빌리티 레벨에 해당하는 최대 충전 수
	int32 GetMaxCharges(int32 Level) const;

	// 쿨타임 중임을 나타내는 태그
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cooldown",
		meta = (Categories = "Cooldown"))
	FGameplayTag CooldownTag;

	// 쿨감 적용 전 기준 초
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cooldown")
	FScalableFloat BaseCooldown = FScalableFloat(0.f);

	// 최대 충전 수. 커브를 안 걸면 전 레벨 1충전
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cooldown")
	FScalableFloat MaxCharges = FScalableFloat(1.f);

private:
	mutable FGameplayTagContainer CooldownTagContainer;
};
