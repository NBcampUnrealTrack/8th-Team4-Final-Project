// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/RogueEffectApplier.h"
#include "ScalableFloat.h"
#include "RogueGameplayAbility.generated.h"

struct FAugmentGrantedAbility;

UCLASS(Abstract)
class FPS_API URogueGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URogueGameplayAbility();

	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	                           const FGameplayAbilityActorInfo* ActorInfo,
	                           const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// 충전이 남았으면 통과
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle,
	                           const FGameplayAbilityActorInfo* ActorInfo,
	                           OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// 액티브 스킬은 State.Combat.ActionBlocked 태그가 존재하면 차단
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                                const FGameplayAbilityActorInfo* ActorInfo,
	                                const FGameplayTagContainer* SourceTags = nullptr,
	                                const FGameplayTagContainer* TargetTags = nullptr,
	                                OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// Event.Skill.Used 발신
	virtual void CommitExecute(const FGameplayAbilitySpecHandle Handle,
	                           const FGameplayAbilityActorInfo* ActorInfo,
	                           const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	// 발동 조건 검사 -> 페이로드 조립 -> 커밋 -> 대가 지불 -> Deliver
	// TriggerEventData 가 없으면 이벤트 관련 조건은 건너뜀
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	// 어떤 경로로 끝나도 장착 태그를 되돌린다 (사용·취소·사망·무기 교체)
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
	                        const FGameplayAbilityActorInfo* ActorInfo,
	                        const FGameplayAbilityActivationInfo ActivationInfo,
	                        bool bReplicateEndAbility, bool bWasCancelled) override;

	// 페이로드 조립 -> 커밋 -> 큐 -> 시전 대기 -> Deliver.
	// bHoldToUse 면 사용 입력이 온 시점에 불린다
	void ExecuteAbility(AActor* EventTarget);

	// 조립된 페이로드를 전달하는 지점. 자식 어빌리티가 구현.
	// 시전자 정보는 CurrentActorInfo 로 얻는다
	virtual void Deliver(const FAugmentGrantedAbility& Entry,
	                     const FRogueEffectPayload& Payload) {}

	// 이 이벤트를 일으킨 무기. 대미지 이벤트는 컨텍스트의 무기 태그, 그 외에는 현재 장착 무기 태그
	FGameplayTag ResolveEventWeaponTag(AActor* Avatar, const FGameplayEventData* TriggerEventData) const;

	// 거리 조건 확인
	static bool PassesDistanceCondition(const UAbilitySystemComponent* SourceASC,
	                                    const FGameplayEventData* TriggerEventData,
	                                    const FAugmentGrantedAbility& Entry);

	// 이벤트가 실어 온 대상. 조건을 못 맞추면 nullptr
	AActor* ResolveEventTarget(AActor* Avatar, const FGameplayEventData* TriggerEventData,
	                           const FAugmentGrantedAbility& Entry) const;

	// 이 어빌리티를 부여한 계열의 GrantedAbilities 항목
	const FAugmentGrantedAbility* FindGrantedEntry(FGameplayAbilitySpecHandle Handle,
	                                               const FGameplayAbilityActorInfo* ActorInfo) const;

	// EffectsToApply 를 전부 로드. 비었거나 하나라도 실패하면 false
	bool LoadEffectClasses(const FAugmentGrantedAbility& Entry,
	                       TArray<TSubclassOf<UGameplayEffect>>& OutClasses) const;

	// 페이로드 조립. GE 로드나 범위 중심 확보에 실패하면 false
	bool BuildEffectPayload(const FAugmentGrantedAbility& Entry, AActor* Avatar, AActor* EventTarget,
	                        FGameplayAbilitySpecHandle Handle,
	                        const FGameplayAbilityActorInfo* ActorInfo,
	                        FRogueEffectPayload& OutPayload) const;

	// SelfCostHealth 만큼 체력을 지불하고 실제로 깎인 양을 반환
	float PayHealthCost(const FGameplayAbilityActorInfo* ActorInfo, int32 Level) const;

	// 시전 대가로 잃는 체력
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cost")
	FScalableFloat SelfCostHealth = FScalableFloat(0.f);

	// true이면 시전 확정 시 Event.Skill.Used 를 발신
	// 액티브 증강만 활성화, 이벤트로 발동하는 증강은 액티브 스킬이 아님
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Activation")
	bool bBroadcastSkillUsed = false;

	// 행동 차단 상태 발동 여부, 이동기 및 탈출기와 같은 예외 용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Activation",
		meta = (EditCondition = "bBroadcastSkillUsed", EditConditionHides))
	bool bIgnoreActionBlock = false;

	// 시전 확정 시 실행할 GameplayCue. 비어 있으면 실행 X
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Activation",
		meta = (Categories = "GameplayCue"))
	FGameplayTag ActivationCueTag;

	// 켜면 입력 즉시 발동하지 않고 손에 든다. 좌/우클릭으로 사용, 같은 키로 취소
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Activation")
	bool bHoldToUse = false;

	// 손에 들고부터 사용 입력을 받기까지의 시간, 장착 모션이 있는 경우
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Activation",
		meta = (EditCondition = "bHoldToUse", EditConditionHides, ClampMin = "0.0"))
	float EquipSeconds = 0.f;

	// 손에 든 동안 유지할 GameplayCue. Looping 큐
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Activation",
		meta = (EditCondition = "bHoldToUse", EditConditionHides, Categories = "GameplayCue"))
	FGameplayTag EquipCueTag;

	// 시전 확정부터 실제 발동까지의 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Activation",
		meta = (ClampMin = "0.0"))
	float CastDelaySeconds = 0.f;

	// 시전 지연 동안 유지할 예고 GameplayCue. 어빌리티가 끝나면 자동 제거
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Activation",
		meta = (EditCondition = "CastDelaySeconds > 0", EditConditionHides, Categories = "GameplayCue"))
	FGameplayTag TelegraphCueTag;

	// 실제 발동 순간 실행할 GameplayCue. 시전 지연이 끝난 뒤 한 번
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Activation",
		meta = (Categories = "GameplayCue"))
	FGameplayTag DeliverCueTag;

	// 사용 후 유틸을 든 상태를 유지하는 시간, 사용 모션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Activation",
		meta = (EditCondition = "bHoldToUse", EditConditionHides, ClampMin = "0.0"))
	float RecoverySeconds = 0.35f;

	// 어느 입력으로 사용했는지. 홀드가 아니면 Primary
	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Activation",
		meta = (Categories = "Event.Utility.Input"))
	FGameplayTag UsedInputTag;

	// 어빌리티 레벨에 해당하는 최대 충전 수
	int32 GetMaxCharges(int32 Level) const;

	// 설정(RogueGASSettings)에 등록된 GE 클래스. 미등록이면 nullptr + Error 로그
	static UClass* GetCooldownEffectClass();
	static UClass* GetStatusEffectClass();

	// 쿨타임 중임을 나타내는 태그
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cooldown",
		meta = (Categories = "Cooldown"))
	FGameplayTag CooldownTag;

	// 쿨감 적용 전 기준 초
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cooldown")
	FScalableFloat BaseCooldown = FScalableFloat(0.f);

	// 최대 충전 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cooldown")
	FScalableFloat MaxCharges = FScalableFloat(1.f);

private:
	// 손에 들기
	void BeginHold();

	// 시전자와 조립된 페이로드의 위치, 방향을 실은 큐 파라미터
	FGameplayCueParameters MakeCueParams() const;

	// 시전자 기준으로 큐 실행
	void ExecuteCue(const FGameplayTag& CueTag);

	void EndSelf(bool bCancelled);

	UFUNCTION()
	void FinishDeliver();

	UFUNCTION()
	void HandleRecoveryFinished();

	// 같은 키 재입력 -> 취소
	UFUNCTION()
	void HandleHoldCancelPress(float TimeWaited);

	// 좌/우클릭 -> 사용
	UFUNCTION()
	void HandleUseInput(FGameplayEventData EventData);

	// 장착 대기가 끝나면 사용 입력을 받기 시작
	UFUNCTION()
	void HandleEquipFinished();

	// 내가 얹은 UtilityHeld 를 아직 안 걷었는지
	bool bHoldTagApplied = false;

	// 발동 중인 계열 항목과 조립된 페이로드
	const FAugmentGrantedAbility* ActiveEntry = nullptr;
	FRogueEffectPayload ActivePayload;

	mutable FGameplayTagContainer CooldownTagContainer;
};
