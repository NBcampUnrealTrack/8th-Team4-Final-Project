// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GAS/Damage/RogueDamageTypes.h"

#include "RogueHealthComponent.generated.h"

class UAbilitySystemComponent;
class URogueVitalAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRogueDeathStateChanged, bool, bIsDead);

UCLASS(ClassGroup = (Rogue), meta = (BlueprintSpawnableComponent))
class FPS_API URogueHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogueHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ASC 는 PlayerState 에 있어 이 컴포넌트의 BeginPlay 시점에 없을 수 있음
	// 서버/클라이언트 양쪽 모두 소유자가 InitAbilityActorInfo 직후에 직접 호출 
	UFUNCTION(BlueprintCallable, Category = "Rogue|Health")
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Health")
	void UninitializeFromAbilitySystem();

	// 라운드 시작 / 리스폰 시 호출. 서버 전용.
	// 상태만 초기화, 체력은 초기화하지 않음
	UFUNCTION(BlueprintCallable, Category = "Rogue|Health")
	void ServerResetDeathState();

	UFUNCTION(BlueprintPure, Category = "Rogue|Health")
	bool IsDead() const { return bIsDead; }

protected:
	virtual void OnUnregister() override;

private:
	UFUNCTION()
	void OnRep_bIsDead();

	// AttributeSet 의 사망 감지 구독, 서버에서만 호출
	void HandleOutOfHealth(UAbilitySystemComponent* VictimASC, const FGameplayEffectContextHandle& ContextHandle);

	// 사망 태그 부여와 아바타 통보를 묶은 서버/클라이언트 공통 경로
	void ApplyDeathStateLocally(bool bDead);

	// 사망 원인에서 연출용 정보를 뽑아 DeathReaction에 채움
	void ServerBuildDeathReaction(const FGameplayEffectContextHandle& ContextHandle);

	// 랙돌/입력 차단은 캐릭터 역할
	// 인터페이스로만 통보, 서버 전용 호출
	void NotifyDeath(const FGameplayEffectContextHandle& ContextHandle) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<URogueVitalAttributeSet> VitalSet;

	FDelegateHandle OutOfHealthHandle;

	// 사망 여부, 서버에서만 변경
	UPROPERTY(ReplicatedUsing = OnRep_bIsDead)
	bool bIsDead = false;

	// 사망 연출용 정보
	UPROPERTY(Replicated)
	FRogueDeathReaction DeathReaction;

public:
	// 모든 머신에서 발신, 킬로그/관전 전환/HUD 가 구독
	UPROPERTY(BlueprintAssignable, Category = "Rogue|Health")
	FOnRogueDeathStateChanged OnDeathStateChanged;
};
