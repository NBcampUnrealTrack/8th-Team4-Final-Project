// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_WeaponAbilityBase.h"
#include "GA_Reload.generated.h"

class UAnimMontage;

// 재장전을 트리거하는 GameplayAbility.
// 실제 재장전 상태 머신(탄약 갱신 타이밍 포함)은 AWeaponBase::StartReload()가 갖고 있고,
// 이 어빌리티는 재장전 시작/완료 시점에 GAS 이벤트(Event.Weapon.ReloadStart/Complete)만 발신한다.
// 재장전 도중 사격 차단은 AWeaponBase::IsReloading()으로 이미 처리되므로 여기서 별도 태그 관리를 하지 않는다.
UCLASS()
class FPS_API UGA_Reload : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
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