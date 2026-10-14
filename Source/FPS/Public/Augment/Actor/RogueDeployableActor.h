// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Augment/Actor/RogueAugmentActor.h"
#include "Interfaces/RogueDamageableTarget.h"
#include "RogueDeployableActor.generated.h"

class URogueAbilitySystemComponent;
class URogueHealthComponent;
class URogueVitalAttributeSet;

// 체력을 갖고 파괴되는 설치물/소환물의 공통 부모
// 자체 ASC 를 들고 있어 기존 히트스캔/대미지 파이프라인이 그대로 적용
UCLASS(Abstract)
class FPS_API ARogueDeployableActor : public ARogueAugmentActor,
                                      public IAbilitySystemInterface,
                                      public IRogueDamageableTarget
{
	GENERATED_BODY()

public:
	ARogueDeployableActor();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Rogue|Deployable")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Rogue|Deployable")
	float GetMaxHealth() const;

	// 로컬 시점 기준 아군/적 윤곽선 갱신
	void RefreshTeamOutline(bool bForceRefresh = true);

	static void RefreshTeamOutlineForAll(UWorld* World);

protected:
	virtual void BeginPlay() override;
	virtual void OnRep_Owner() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void HandleDeath_Implementation(const FRogueDeathContext& Context) override;

	// 스폰 시 최대 체력
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Deployable", meta = (ClampMin = "1.0"))
	float DeployHealth = 100.f;

	// 파괴 순간 재생할 GameplayCue. 비어 있으면 실행 X
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Deployable",
		meta = (Categories = "GameplayCue"))
	FGameplayTag DestroyedCueTag;

	// 초당 전진 거리(cm). 0 이면 정지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Deployable", meta = (ClampMin = "0.0"))
	float MoveSpeed = 0.f;

	// 시선의 상하 성분을 버리고 수평으로만 전진
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Deployable",
		meta = (EditCondition = "MoveSpeed > 0", EditConditionHides))
	bool bMoveHorizontally = true;

	// 파괴 GameplayCue 재생을 위한 유예
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Deployable", meta = (ClampMin = "0.0"))
	float DestroyDelaySeconds = 0.2f;

private:
	int32 CachedTeamOutlineState = INDEX_NONE;

	void HandleRevealedChanged(const FGameplayTag Tag, int32 NewCount);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rogue|Deployable",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URogueAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<URogueVitalAttributeSet> VitalSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rogue|Deployable",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URogueHealthComponent> HealthComponent;

	FVector MoveDirection = FVector::ZeroVector;

	bool bDestroyHandled = false;
};
