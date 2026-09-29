// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "RogueAugmentStatResolver.generated.h"

class URogueAugmentAttributeSet;

UCLASS()
class FPS_API URogueAugmentStatResolver : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ------------------------------------
	// 전투 관련

	// 분당 발사 수. 쿨다운 GE 의 SetDuration 에 쓸 초는 60 / 이 값
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Weapon")
	static float GetFinalRPM(const AActor* Actor, float BaseRPM);

	// 재장전에 걸리는 시간
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Weapon")
	static float GetFinalReloadTime(const AActor* Actor, float BaseReloadTime);

	// 탄창 크기, 소수일 경우 내림
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Weapon")
	static int32 GetFinalMagazineSize(const AActor* Actor, int32 BaseMagazineSize);

	// 산탄 수
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Weapon")
	static int32 GetFinalPelletCount(const AActor* Actor, int32 BasePelletCount);

	// 유효 사거리 (m)
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Weapon")
	static float GetFinalEffectiveRange(const AActor* Actor, float BaseRangeMeters);

	// 관통력
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Weapon")
	static float GetFinalPenetration(const AActor* Actor, float BasePenetration);

	// 근접 공격 사거리 (m)
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Weapon")
	static float GetFinalMeleeRange(const AActor* Actor, float BaseRangeMeters);

	// 탄퍼짐
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Accuracy")
	static float GetFinalSpread(const AActor* Actor, float BaseSpread);

	// 반동
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Accuracy")
	static float GetFinalRecoil(const AActor* Actor, float BaseRecoil);

	// ------------------------------------
	// 이동 관련 
	
	// 이동 속도
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Movement")
	static float GetFinalMoveSpeed(const AActor* Actor, float BaseSpeed);

	// 달리기 속도
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Movement")
	static float GetFinalSprintSpeed(const AActor* Actor, float BaseSprintSpeed);

	// 점프 속도. 점프 높이는 점프 속도의 제곱에 비례
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Movement")
	static float GetFinalJumpVelocity(const AActor* Actor, float BaseJumpVelocity);

	// 점프 횟수. 0 이면 1회 점프만 가능
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Movement")
	static int32 GetExtraJumpCount(const AActor* Actor);

	// -----------------------------------
	// 증강 관련
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Skill")
	static float GetFinalCooldown(const AActor* Actor, float BaseCooldownSeconds);

	// -----------------------------------
	// 드래프트 관련

	// 추가 리롤 횟수
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Draft")
	static int32 GetBonusRerollCount(const AActor* Actor);

	// 잃은 체력 비례 보너스. 잃은 체력 2% 마다 1단계, 최대 40단계
	static float CalcLostHealthBonus(float Health, float MaxHealth, float RatePerStep);

	// 증강 무작위 공용. Min ~ Max 사이 값
	static float RollRange(float Min, float Max);

private:
	// Character / PlayerState / Controller 중 무엇을 넘겨도 해석
	// 없으면 nullptr
	static const URogueAugmentAttributeSet* FindAugmentSet(const AActor* Actor);
};
