// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
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

	// 어느 입력으로 소환됐는지. 의미는 각 액터가 정한다. FinishSpawning 전에 세팅
	void SetUsedInputTag(const FGameplayTag& InUsedInputTag) { UsedInputTag = InUsedInputTag; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 페이로드를 자기 위치에서 적용. 신관 만료나 원격 기폭에서 호출
	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment|Actor")
	void Detonate();

protected:
	virtual void BeginPlay() override;

	// 페이로드의 적용 반경(cm), 범위는 클라이언트에게도 보여야하므로 복제
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rogue|Augment|Actor")
	float EffectRadius = 0.f;

	// 소환한 입력. 궤적이나 배치가 달라지므로 클라이언트도 알아야 한다
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rogue|Augment|Actor",
		meta = (Categories = "Event.Utility.Input"))
	FGameplayTag UsedInputTag;

	// 자동 기폭 시작
	void StartFuse();

	// 스폰 직후 기폭을 시작할지 여부
	virtual bool ShouldStartFuseOnBeginPlay() const { return bAutoDetonate; }

	// 기폭 순간 재생할 GameplayCue. 비어 있으면 실행 X
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Actor",
		meta = (Categories = "GameplayCue"))
	FGameplayTag DetonateCueTag;

	// 스폰 순간 재생할 GameplayCue. 지속형 설치물의 유지 연출용. 비어 있으면 실행 X
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Actor",
		meta = (Categories = "GameplayCue"))
	FGameplayTag SpawnCueTag;

	// 연출 훅. 기폭이 확정된 뒤 모든 클라이언트에서 실행
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Augment|Actor")
	void OnDetonated();

	// 마지막으로 터진 지점
	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Augment|Actor")
	FVector LastDetonateLocation = FVector::ZeroVector;

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

	// 터진 위치에 남길 액터
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Actor",
		meta = (EditCondition = "DetonateInterval <= 0", EditConditionHides))
	TSoftClassPtr<ARogueAugmentActor> SpawnOnDetonateClass;

	// 0 보다 크면 스폰 지점 기준 이 반경(cm) 안 무작위 지점에서 터짐
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Scatter", meta = (ClampMin = "0.0"))
	float ScatterRadius = 0.f;

	// 무작위 지점을 지면으로 내려붙일지. 공중 폭발 방지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Scatter",
		meta = (EditCondition = "ScatterRadius > 0", EditConditionHides))
	bool bScatterSnapToGround = true;

	// 지면 탐색 트레이스 길이(cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Scatter",
		meta = (EditCondition = "ScatterRadius > 0 && bScatterSnapToGround", EditConditionHides, ClampMin = "0.0"))
	float ScatterTraceDistance = 5000.f;

private:
	// 터진 위치에 남길 액터 스폰
	void SpawnFollowUpActor();

	// 이번 기폭이 적용될 지점. 산포가 없으면 액터 위치
	FVector ResolveDetonateOrigin() const;

	// 기폭 큐 재생. 모든 클라이언트에서 각자 실행
	void PlayDetonateCue(const FVector& Location) const;

	// 스폰 큐 재생. 액터가 복제되므로 각 클라이언트의 BeginPlay 에서 실행
	void PlaySpawnCue() const;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Detonated(FVector_NetQuantize Location);

	// 서버 전용
	FRogueEffectPayload Payload;

	bool bDetonated = false;

	// 산포의 기준점. 현재 위치를 쓰면 포격이 한쪽으로 흘러간다
	FVector ScatterAnchor = FVector::ZeroVector;

	FTimerHandle FuseTimer;
	FTimerHandle LifeTimer;
};
