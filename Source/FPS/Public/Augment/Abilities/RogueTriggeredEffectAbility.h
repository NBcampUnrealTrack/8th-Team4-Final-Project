// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Augment/Abilities/RogueGameplayAbility.h"
#include "RogueTriggeredEffectAbility.generated.h"

class UGameplayEffect;
struct FAugmentGrantedAbility;

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

private:
	// 이 어빌리티를 부여한 계열의 GrantedAbilities 항목
	const FAugmentGrantedAbility* FindGrantedEntry(FGameplayAbilitySpecHandle Handle,
	                                               const FGameplayAbilityActorInfo* ActorInfo) const;

	// 이 이벤트를 일으킨 무기. 대미지 이벤트는 컨텍스트의 무기 태그 사용, 그 외에는 현재 장착 무기 태그 사용
	FGameplayTag ResolveEventWeaponTag(AActor* Avatar, const FGameplayEventData* TriggerEventData) const;

	// 효과를 받을 액터. 조건을 못 맞추면 nullptr
	AActor* ResolveTarget(AActor* Avatar, const FGameplayEventData* TriggerEventData,
	                      const FAugmentGrantedAbility& Entry) const;

	// 증강 정의의 해당 레벨 SetByCaller 수치를 스펙에 옮긴다
	void InjectLevelMagnitudes(const FGameplayEffectSpecHandle& SpecHandle,
	                           FGameplayAbilitySpecHandle Handle,
	                           const FGameplayAbilityActorInfo* ActorInfo) const;
};
