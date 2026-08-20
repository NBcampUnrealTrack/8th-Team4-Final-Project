// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Augment/Abilities/RogueGameplayAbility.h"
#include "RogueTriggeredEffectAbility.generated.h"

class UGameplayEffect;

// 이벤트가 오면 GE 를 붙이고 끝나는 증강들의 공통 부모.
UCLASS(Abstract)
class FPS_API URogueTriggeredEffectAbility : public URogueGameplayAbility
{
	GENERATED_BODY()

public:
	URogueTriggeredEffectAbility();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	// 이벤트가 왔을 때 적용할 GE
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Trigger")
	TArray<TSoftClassPtr<UGameplayEffect>> EffectsToApply;

	// 켜면 이벤트가 실어 온 대상에게, 끄면 자신에게 적용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Trigger")
	bool bApplyToEventTarget = false;

	// 대상에게 걸 때 적만 허용. 아군 오폭 방지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Trigger",
		meta = (EditCondition = "bApplyToEventTarget"))
	bool bHostileTargetOnly = true;

	// 이 무기로 발생한 이벤트만 발동. 비어 있으면 무기 조건 없음.
	// 대미지 이벤트는 컨텍스트의 무기 태그 사용, 그 외에는 현재 장착 무기 태그 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Trigger",
		meta = (Categories = "Weapon.Type"))
	FGameplayTagContainer RequiredWeaponTags;

private:
	// 이 이벤트를 일으킨 무기. 대미지 이벤트는 컨텍스트의 무기 태그 사용, 그 외에는 현재 장착 무기 태그 사용
	FGameplayTag ResolveEventWeaponTag(AActor* Avatar, const FGameplayEventData* TriggerEventData) const;

	// 효과를 받을 액터. 조건을 못 맞추면 nullptr
	AActor* ResolveTarget(AActor* Avatar, const FGameplayEventData* TriggerEventData) const;

	// 증강 정의의 해당 레벨 SetByCaller 수치를 스펙에 옮긴다
	void InjectLevelMagnitudes(const FGameplayEffectSpecHandle& SpecHandle,
	                           FGameplayAbilitySpecHandle Handle,
	                           const FGameplayAbilityActorInfo* ActorInfo) const;
};
