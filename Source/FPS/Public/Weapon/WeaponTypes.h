// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WeaponTypes.generated.h"

class UCurveVector;

// 런타임 무기 슬롯 식별용.
// 슬롯은 1/2/3번 입력 및 Primary/Secondary/Melee 인스턴스 보관에만 사용한다.
UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	None UMETA(Hidden),
	Primary UMETA(DisplayName = "Primary"),
	Secondary UMETA(DisplayName = "Secondary"),
	Melee UMETA(DisplayName = "Melee")
};

// 탄퍼짐/반동 튜닝 값 한 세트. 기획(밸런스) 담당자가 코드/블루프린트를 열지 않고도
// DataTable 행만 수정해서 에디터에서 바로 테스트할 수 있도록 분리한 값들이다.
// AWeaponBase::SpreadRecoilTable + SpreadRecoilRowName이 지정되면 BeginPlay 시 이 값들로
// BaseSpreadDegrees 등 개별 프로퍼티를 덮어쓴다. 미지정 시에는 무기 블루프린트에 직접
// 설정된 기존 값을 그대로 쓴다(하위 호환 fallback).
USTRUCT(BlueprintType)
struct FWeaponSpreadRecoilRow : public FTableRowBase
{
	GENERATED_BODY()

	// --- 탄퍼짐 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread")
	float BaseSpreadDegrees = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread")
	float BloomPerShotDegrees = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread")
	float MaxBloomDegrees = 5.f;

	// 실제 발사 간격이 끝난 뒤 블룸 회복까지의 여유 시간(초). 연사 틱의 작은 지연을 흡수한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread", meta = (ClampMin = "0.0", Units = "s"))
	float BloomRecoveryDelay = 0.05f;

	// 최대 블룸에서 0까지 걸리는 회복 시간(초). 작은 블룸은 같은 속도로 더 빨리 회복한다.
	// 0이면 위 대기 시간이 지난 시점에 사격 블룸만 즉시 0이 된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread", meta = (ClampMin = "0.0", Units = "s"))
	float BloomRecoveryTime = 0.15f;

	// 기존 BP/DT 직렬화 호환용. 새 회복 계산에는 사용하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use BloomRecoveryDelay; measured after the shot interval."))
	float SpreadDecayDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use BloomRecoveryTime; seconds from maximum bloom to zero."))
	float ConsecutiveShotDecayPerSecond = 5.f;

	// ADS 중 이동으로 브레이킹(정지)에 실패했을 때 적용되는 탄퍼짐(도). 정지(브레이킹 성공) 중에는
	// 여전히 0으로 고정된다 - 조준해도 움직이면서 쏘면 빗나가야 한다는 요구사항 대응.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread")
	float MovingADSSpreadDegrees = 2.f;

	// 앉아서 쏠 때 탄퍼짐에 곱해지는 배율(1=영향 없음). 공중 페널티에는 적용되지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrouchSpreadMultiplier = 0.7f;

	// --- 반동 (전투 시스템 명세서 2.1/2.2) ---
	// X채널=Yaw, Y채널=Pitch. WeaponBase::RecoilCurve 주석 참고.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Recoil")
	TObjectPtr<UCurveVector> RecoilCurve;

	// 매 프레임 Yaw 델타에 추가되는 무작위 편차 범위(도, ±).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Recoil")
	float RecoilRandomness = 0.f;

	// RecoilCurve의 Yaw(X채널) 결과에 곱해지는 배율. 0으로 두면 이 무기는 좌우 반동이 전혀 없고
	// Pitch(상하) 반동만 남는다 - 예: 스나이퍼는 X축 반동을 없애고 싶을 때 0으로 설정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Recoil")
	float RecoilYawMultiplier = 1.f;

	// 누적 가능한 최대 상방 반동(도). 초과분은 Clamp.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Recoil")
	float RecoilMaxPitch = 10.f;

	// 사격 중단 후 이 시간(초)이 지나야 반동 복귀가 시작된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Recoil")
	float RecoilRecoveryDelay = 0.15f;

	// 반동 복귀 FInterpTo 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Recoil")
	float RecoilRecoverySpeed = 10.f;
};