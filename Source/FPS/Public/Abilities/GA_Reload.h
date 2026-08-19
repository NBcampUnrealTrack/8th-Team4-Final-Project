// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_WeaponAbilityBase.h"
#include "GA_Reload.generated.h"

class UAnimMontage;

// 재장전을 트리거하는 GameplayAbility.
// 실제 탄약 갱신과 타이머는 AWeaponBase가 담당하고, 이 Ability는 재장전 이벤트와
// State.Combat.IsReloading 태그의 수명주기를 Ability 활성 시간에 맞춰 관리한다.
UCLASS()
class FPS_API UGA_Reload : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Reload();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                                 const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
	                                 FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                              const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 재장전 시 재생할 몽타주.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Animation")
	TObjectPtr<UAnimMontage> ReloadMontage;

private:
	UFUNCTION()
	void HandleReloadFinished();
};