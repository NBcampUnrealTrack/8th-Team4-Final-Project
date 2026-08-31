// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GAS/RogueEffectApplier.h"
#include "RogueAugmentActor.generated.h"

// 어빌리티가 조립한 페이로드를 들고 있다가 자기 자리에서 터뜨리는 액터
UCLASS(Abstract)
class FPS_API ARogueAugmentActor : public AActor
{
	GENERATED_BODY()

public:
	ARogueAugmentActor();

	// 스폰 직후 어빌리티가 서버에서 한 번 호출
	void InitializePayload(const FRogueEffectPayload& InPayload);

	// 페이로드를 자기 위치에서 적용. 신관 만료나 원격 기폭에서 호출
	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment|Actor")
	void Detonate();

protected:
	virtual void BeginPlay() override;

	// 자동 기폭 시작
	void StartFuse();

	// 스폰 직후 기폭을 시작할지 여부
	virtual bool ShouldStartFuseOnBeginPlay() const { return bAutoDetonate; }

	// 연출 훅. 기폭이 확정된 뒤 모든 클라이언트에서 실행
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Augment|Actor")
	void OnDetonated();

	// 끄면 외부에서 Detonate 를 부를 때까지 기다림
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Actor")
	bool bAutoDetonate = true;

	// 첫 기폭까지의 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Actor",
		meta = (EditCondition = "bAutoDetonate", EditConditionHides, ClampMin = "0.0"))
	float FuseSeconds = 0.f;

	// 0 보다 크면 이 주기로 반복 기폭. 화염 지대나 치유 토템처럼 지속되는 것
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Actor", meta = (ClampMin = "0.0"))
	float DetonateInterval = 0.f;

	// 0 보다 크면 이 시간 뒤 소멸
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Actor", meta = (ClampMin = "0.0"))
	float LifeSeconds = 0.f;

	// 1회 기폭형이 터진 뒤 사라질지 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Actor",
		meta = (EditCondition = "DetonateInterval <= 0", EditConditionHides))
	bool bDestroyOnDetonate = true;

private:
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Detonated();

	// 서버 전용
	FRogueEffectPayload Payload;

	bool bDetonated = false;

	FTimerHandle FuseTimer;
	FTimerHandle LifeTimer;
};
