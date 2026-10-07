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
	                     const FRogueEffectPayload& Payload) override;

	// 시전자의 전방으로 SpawnForwardOffset 만큼 떨어진 위치에 스폰
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Spawn", meta = (ClampMin = "0.0"))
	float SpawnForwardOffset = 100.f;

	// 켜면 조준점(최대 MaxAimDistance)에 스폰. 닿지 않으면 최대 사거리 지점의 지면
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Spawn")
	bool bSpawnAtAimPoint = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Spawn",
		meta = (EditCondition = "bSpawnAtAimPoint", EditConditionHides, ClampMin = "0.0", Units = "cm"))
	float MaxAimDistance = 4000.f;

private:
	FTransform ResolveAimSpawnTransform(const AActor* Avatar, const FVector& EyeLocation, const FRotator& EyeRotation) const;
};
