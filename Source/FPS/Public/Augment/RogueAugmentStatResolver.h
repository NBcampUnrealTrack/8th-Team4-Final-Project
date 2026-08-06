// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "RogueAugmentStatResolver.generated.h"

class URogueAugmentAttributeSet;

/**
 * 증강 보정을 적용한 최종 수치를 돌려준다.
 * 타 팀이 증강 시스템을 만나는 유일한 창구다.
 *
 * ASC 나 AttributeSet 이 없으면 기본값을 그대로 반환한다.
 * ARoguePlayerState 가 아직 없는 지금도 타 팀이 최종 API 로 코딩할 수 있고,
 * 나중에 ASC 가 붙으면 값만 달라지고 호출부는 그대로다.
 *
 * 대미지 계수 12축은 여기 없다. ExecCalc_Damage 안쪽에서만 쓰이므로
 * 타 팀 시야에 들어올 이유가 없다.
 *
 * 시간 축(재장전·쿨다운)은 여기서 나눗셈까지 끝낸다.
 * 배율을 그대로 넘겨주면 호출부가 곱해버려 조용히 반대로 걸린다
 */
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

	// 내가 내는 발소리
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Movement")
	static float GetFinalFootstepVolume(const AActor* Actor, float BaseVolume);

	// 내가 남의 소리를 듣는 범위
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Movement")
	static float GetFinalHearingRange(const AActor* Actor, float BaseRange);

	// -----------------------------------
	// 증강 관련
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Skill")
	static float GetFinalCooldown(const AActor* Actor, float BaseCooldownSeconds);

	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Skill")
	static float GetFinalThrowRange(const AActor* Actor, float BaseRangeMeters);

	// Actor 는 회복을 '받는' 쪽
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment|Defense")
	static float GetFinalHealingReceived(const AActor* Actor, float BaseHealAmount);

private:
	// Character / PlayerState / Controller 중 무엇을 넘겨도 해석
	// 없으면 nullptr — 호출부는 기본값을 그대로 쓴다
	static const URogueAugmentAttributeSet* FindAugmentSet(const AActor* Actor);
};
