// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RPMUtils.generated.h"

class UAbilitySystemComponent;

// GAS 스탯/증강을 반영한 최종 RPM을 계산하는 유틸리티.
// AWeaponBase는 GAS를 모르므로, 이 값은 반드시 GA_RangedAttack이 호출해서
// AWeaponBase::SetEffectiveRPM으로 대신 넣어준다.
UCLASS()
class FPS_API URPMUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// TODO(GAS 담당자 확인 필요): 증강/스탯 보정이 반영된 최종 RPM 계산 로직을
	// 실제로 누가/어디서 구현할지 협의 필요. 현재는 BaseRPM을 그대로 반환하는 임시 스텁이다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	static float GetFinalRPM(const UAbilitySystemComponent* ASC, float BaseRPM);
};