// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameMode/Types/DominationTypes.h"
#include "GameplayEffectTypes.h"
#include "GAS/Damage/RogueDamageTypes.h"

#include "RogueHealthComponent.generated.h"

class AActor;
class UAbilitySystemComponent;
class URogueVitalAttributeSet;
struct FGameplayEventData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRogueDeathStateChanged, bool, bIsDead);
// 피격 방향 인디케이터 UI 전용. 생사와 무관하게 매 피격마다(로컬 소유 클라이언트에서만) 발신된다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRogueHitReaction, FVector, Direction);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRogueDamageTakenNative, const AActor*, FVector);

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

	// Populated from the lethal effect before OnDeathStateChanged is broadcast.
	const FRogueDeathReaction& GetDeathReaction() const { return DeathReaction; }

protected:
	virtual void OnUnregister() override;

private:
	UFUNCTION()
	void OnRep_bIsDead();

	UFUNCTION()
	void OnRep_HitReaction();

	// AttributeSet 의 사망 감지 구독, 서버에서만 호출
	void HandleOutOfHealth(UAbilitySystemComponent* VictimASC, const FGameplayEffectContextHandle& ContextHandle);

	// Event_Combat_DamageTaken 구독, 서버에서만 호출. 생사와 무관하게 매 피격마다 들어온다.
	void HandleDamageTakenEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

	// ContextHandle에서 피격자 -> 가해자 방향을 뽑는다. HitShotDirection이 없으면 Instigator 위치로 폴백한다.
	// ServerBuildDeathReaction의 방향 계산 로직과 동일한 폴백 규칙을 쓴다.
	FVector ComputeHitDirection(const FGameplayEffectContextHandle& ContextHandle, const AActor* InstigatorActor) const;

	// 마지막 재생 이후 PainSoundCooldownSeconds가 지났을 때만 고통 사운드를 재생한다 - 순식간에
	// 여러 발 맞아도(펠릿, 동시 피격 등) 최초 1회만 나고, 시간차를 두고 맞으면 매번 다시 난다.
	// 서버에서만 호출.
	void PlayPainSoundIfDue();

	// 소유자의 EDominationCombatClass를 조회한다(비-Domination 모드/미선택이면 Unselected 폴백).
	EDominationCombatClass ResolveOwnerCombatClass() const;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayPainSound(EDominationCombatClass CombatClass, int32 SoundIndex, FVector Location);

	// 사망 태그 부여와 아바타 통보를 묶은 서버/클라이언트 공통 경로
	void ApplyDeathStateLocally(bool bDead);

	// 사망 원인에서 연출용 정보를 뽑아 DeathReaction에 채움
	void ServerBuildDeathReaction(const FGameplayEffectContextHandle& ContextHandle);

	// 랙돌/입력 차단은 캐릭터 역할
	// 인터페이스로만 통보, 서버 전용 호출
	void NotifyDeath(const FGameplayEffectContextHandle& ContextHandle) const;

public:
	// 연속 피격 시 고통 사운드가 매 타격마다 겹쳐 나지 않도록 하는 최소 재생 간격(초).
	// 이 시간 안에 또 맞으면 사운드를 스킵하고, 지나서 맞으면 다시 재생한다 - 값이 자연스러운
	// 체감을 좌우하니 기획 피드백에 맞춰 이 값만 조정하면 된다.
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Health|Pain")
	float PainSoundCooldownSeconds = 0.25f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<URogueVitalAttributeSet> VitalSet;

	FDelegateHandle OutOfHealthHandle;
	FDelegateHandle DamageTakenEventHandle;

	// 고통 사운드 쿨다운 기준 시각(서버 월드 타임). 비복제 - 서버만 판단하면 된다.
	double LastPainSoundTime = -1000.0;

	// 사망 여부, 서버에서만 변경
	UPROPERTY(ReplicatedUsing = OnRep_bIsDead)
	bool bIsDead = false;

	// 사망 연출용 정보
	UPROPERTY(Replicated)
	FRogueDeathReaction DeathReaction;

	// 피격 방향 인디케이터용 최소 정보. 본인 화면에만 필요하므로 COND_OwnerOnly로 복제한다.
	UPROPERTY(ReplicatedUsing = OnRep_HitReaction)
	FRogueHitReactionInfo HitReaction;

public:
	// 모든 머신에서 발신, 킬로그/관전 전환/HUD 가 구독
	UPROPERTY(BlueprintAssignable, Category = "Rogue|Health")
	FOnRogueDeathStateChanged OnDeathStateChanged;

	FOnRogueDamageTakenNative OnDamageTakenNative;

	// 로컬 소유 클라이언트(및 리슨서버)에서만 발신 - 피격 방향 인디케이터 UI가 구독한다.
	UPROPERTY(BlueprintAssignable, Category = "Rogue|Health")
	FOnRogueHitReaction OnHitReactionReceived;
};
