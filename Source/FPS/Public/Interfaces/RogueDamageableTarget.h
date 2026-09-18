// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "UObject/Interface.h"

#include "RogueDamageableTarget.generated.h"

class UCombatHitZoneDefinition;

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class URogueDamageableTarget : public UInterface
{
	GENERATED_BODY()
};

class FPS_API IRogueDamageableTarget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Rogue|Damage")
	void HandleDeath(const FRogueDeathContext& Context);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rogue|Damage")
	UCombatHitZoneDefinition* GetCombatHitZoneDefinition() const;

	// 피격 대상이 자신의 히트스캔 임팩트 파티클 표시 여부를 결정한다.
	// 일반 전투 대상은 기존 동작을 유지하고, 훈련용 타겟처럼 비혈흔 연출이 필요한 대상만 override한다.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rogue|Damage|Presentation")
	bool ShouldSpawnHitscanImpactParticle() const;
	virtual bool ShouldSpawnHitscanImpactParticle_Implementation() const { return true; }

	// 모든 머신에서 호출, 사망 랙돌
	UFUNCTION(BlueprintNativeEvent, Category = "Rogue|Damage")
	void PlayDeathReaction(const FRogueDeathReaction& Reaction);
};
