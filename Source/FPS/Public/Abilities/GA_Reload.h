// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_WeaponAbilityBase.h"
#include "GA_Reload.generated.h"

class UAnimMontage;
class AWeaponBase;

// 재장전을 트리거하는 GameplayAbility.
// 실제 재장전 상태 머신(탄약 갱신 타이밍 포함)은 AWeaponBase::StartReload()가 갖고 있고,
// 이 어빌리티는 재장전 시작/완료 시점에 GAS 이벤트(Event.Weapon.ReloadStart/Complete)만 발신한다.
// 재장전 도중 사격 차단은 여전히 AWeaponBase::IsReloading()이 담당한다. ActivationOwnedTags로 붙인
// State.Combat.IsReloading은 그 판정을 대체하지 않으며, 어빌리티 차단(ActivationBlockedTags)이나
// 애니메이션/UI 등 GAS 쪽에서 재장전 상태를 조회해야 하는 다른 시스템을 위한 태그다.
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
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	// 재장전 시 재생할 몽타주.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Animation")
	TObjectPtr<UAnimMontage> ReloadMontage;

private:
	TWeakObjectPtr<AWeaponBase> ReloadingWeapon;
	UFUNCTION()
	void HandleReloadFinished();
};
