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

	// 조립된 페이로드를 전달하는 지점. 자식이 어빌리티가 구현
	virtual void Deliver(const FRogueEffectPayload& Payload,
	                     FGameplayAbilitySpecHandle Handle,
	                     const FGameplayAbilityActorInfo* ActorInfo,
	                     const FGameplayAbilityActivationInfo ActivationInfo,
	                     const FGameplayEventData* TriggerEventData) {}

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
	mutable FGameplayTagContainer CooldownTagContainer;
};
