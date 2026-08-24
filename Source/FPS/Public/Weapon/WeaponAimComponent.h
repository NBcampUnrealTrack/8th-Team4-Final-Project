// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponAimComponent.generated.h"

class UCurveVector;
class UDataTable;

// ADS 상태 + 탄퍼짐(블룸) + ADS 반동을 전담한다. 전투 시스템 명세서 3.3.1의 원칙("지향 사격은
// 탄퍼짐만, ADS는 반동만")대로 이 셋은 bIsADS 하나를 기준으로 서로 배타적으로 갈리는 한 묶음의
// 개념이라 하나의 컴포넌트로 분리했다. AWeaponBase는 "언제" 갱신할지(Fire()/Tick() 호출 시점)만
// 알려주고, AWeaponBase의 기존 public 함수(SetADS/IsADS/GetCurrentSpreadDegrees 등)는 전부 이
// 컴포넌트로 위임하는 얇은 껍데기로 남아 있어 외부 호출부(GA_ADS/CombatComponent/ShooterReticle
// 등)는 변경이 필요 없다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UWeaponAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponAimComponent();

	// --- ADS ---
	// bCanADS가 false인 무기(샷건/칼 등)는 true를 넘겨도 무시되고 계속 지향 사격 상태로 남는다.
	// ADS 해제 시 반동 값을 즉시 0으로 리셋한다(서서히 복귀하지 않음).
	void SetADS(bool bNewIsADS);
	bool IsADS() const { return bIsADS; }
	bool CanADS() const { return bCanADS; }
	float GetADSFieldOfView() const { return ADSFieldOfView; }

	// --- 탄퍼짐 ---
	void SetMovementSpreadMultiplier(float NewMultiplier);
	void SetEffectiveSpreadDegrees(float NewSpreadDegrees);
	float GetBaseSpreadDegrees() const { return BaseSpreadDegrees; }
	// 지금 이 순간 실제로 적용 중인 탄퍼짐 각도(도). ADS/연사 블룸/이동 배율/증강까지 전부 반영된 최종값.
	float GetCurrentSpreadDegrees() const;

	// --- 반동 ---
	void SetEffectiveRecoilMultiplier(float NewMultiplier);

	// AWeaponBase::Tick()에서 매 프레임 호출한다. 탄퍼짐 감쇠 + ADS 반동 생성/회복을 전부 처리한다.
	void TickAim(float DeltaTime);
	// AWeaponBase::Fire() 성공 시(AdvanceFireCounters) 호출한다. 지향 사격일 때만 블룸을 늘리고,
	// ADS/지향 공용으로 TimeSinceLastShot을 리셋한다(스프레드 감쇠, 반동 생성/회복 타이밍의 기준).
	void NotifyShotFired();
	// GA_Reload 진입/무기 교체 등 AWeaponBase가 반동을 즉시 0으로 되돌려야 할 때 호출한다.
	void ResetRecoilState();

protected:
	virtual void BeginPlay() override;

	// SpreadRecoilTable + SpreadRecoilRowName이 지정된 경우, 해당 행의 값으로 아래 탄퍼짐/반동
	// 튜닝 값들을 덮어쓴다. BeginPlay에서 호출된다.
	void ApplySpreadRecoilTuningTable();

	float CalculateCurrentSpreadDegrees() const;
	void TickSpreadDecay(float DeltaTime);

	// ADS 중 사격 유지 구간(TimeSinceLastShot < RecoilRecoveryDelay)에 매 프레임 호출된다. RecoilCurve
	// (UCurveVector, X=Yaw/Y=Pitch)를 FiringElapsedTime으로 계속 evaluate해 이전 프레임과의 델타만
	// 컨트롤러에 적용한다 - 발당 1회가 아니라 시간 기반 연속 적용이라 프레임/RPM과 무관하게 항상 같은
	// 반동 패턴을 그린다.
	void TickProceduralRecoilGeneration(float DeltaTime);
	void TickRecoilRecovery(float DeltaTime);

	// --- 탄퍼짐/반동 DT 튜닝 ---
	// 지정하면 BeginPlay 시 FWeaponSpreadRecoilRow의 값으로 아래 탄퍼짐/반동 튜닝 값들을 덮어쓴다.
	// 기획 담당자가 코드/블루프린트를 건드리지 않고 이 DataTable 행만 수정해서 에디터에서 바로
	// 테스트/조정할 수 있게 하기 위함. 비어있으면(=미지정) 아래 기본값/BP 오버라이드를 그대로 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Tuning")
	TObjectPtr<UDataTable> SpreadRecoilTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Tuning")
	FName SpreadRecoilRowName;

	// --- ADS 튜닝 값 ---
	// ADS 가능한 무기인지 여부. 샷건/칼처럼 ADS가 없는 무기는 false로 설정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS")
	bool bCanADS = true;

	// ADS 중 목표 카메라 FOV(도). 무기마다 줌 배율이 다르므로(예: 저격소총이 가장 좁게 줌) 여기서
	// 값을 갖는다. CombatComponent가 bAiming/CanADS()를 보고 이 값과 평상시 FOV 사이를 보간한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float ADSFieldOfView = 60.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|ADS")
	bool bIsADS = false;

	// --- 탄퍼짐 튜닝 값 ---
	// 무기 기본 탄퍼짐 각도. SpreadRecoilTable 미지정 시 사용되는 기본값(=fallback).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float BaseSpreadDegrees = 1.f;

	// 현재 적용 중인 실제 기본 탄퍼짐 각도. SetEffectiveSpreadDegrees로만 갱신되며, 기본값은
	// (SpreadRecoilTable 적용 이후의) BaseSpreadDegrees와 동일하게 초기화된다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float EffectiveBaseSpreadDegrees = 1.f;

	// 연속 사격 시 사격당 누적되는 탄퍼짐 각도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float BloomPerShotDegrees = 0.2f;

	// 연사 블룸 누적 최대치
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float MaxBloomDegrees = 5.f;

	// 사격을 멈춘 뒤 이 시간(초)이 지나야 ConsecutiveShotCount 감소가 시작된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float SpreadDecayDelay = 0.3f;

	// 초당 감소하는 연속 사격 카운트 (예: 5 = 초당 5발씩 감소)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float ConsecutiveShotDecayPerSecond = 5.f;

	// 이동 상태 탄퍼짐 배율. SetMovementSpreadMultiplier로만 갱신된다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float MovementSpreadMultiplier = 1.f;

	// 연속 사격 횟수. 탄퍼짐 블룸 계산에 사용.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	int32 ConsecutiveShotCount = 0;

	// --- 반동 튜닝 값 (전투 시스템 명세서 2.1/2.2) ---
	// ADS 반동 커브. X채널=Yaw(좌우, +우측), Y채널=Pitch(상하, +위쪽). In 값은 "연속 사격 시작 후
	// 경과 시간(초)"이고, 값은 매 사격마다 더하는 게 아니라 그 시점까지 쌓여야 할 절대 누적
	// 오프셋이다 - TickProceduralRecoilGeneration이 매 프레임 다시 evaluate해서 이전 프레임과의
	// 차이만 적용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	TObjectPtr<UCurveVector> RecoilCurve;

	// 매 프레임 Yaw 델타에 추가되는 무작위 편차 범위(도, ±). Pitch는 커브값을 그대로 따르는 결정론적
	// 패턴으로 남겨 학습 가능하게 하고, Yaw만 소폭 랜덤화해 완전한 예측 사격을 막는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilRandomness = 0.f;

	// 누적 가능한 최대 상방 반동(도). 초과분은 Clamp - 연사 중 반동이 무한정 커지지 않고 이 이상은
	// 좌우 흔들림(RecoilRandomness)만 남는 상한선 역할을 한다. Yaw에는 상한이 없다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilMaxPitch = 10.f;

	// 사격 중단 후 이 시간(초)이 지나야 반동 복귀가 시작된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilRecoveryDelay = 0.15f;

	// 반동 복귀 FInterpTo 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilRecoverySpeed = 10.f;

	// 현재 적용 중인 실제 반동 배율. SetEffectiveRecoilMultiplier로만 갱신되며, RecoilCurve 샘플링
	// 값에 곱해진다. 기본값 1이면 배율 없음(원본 커브 그대로).
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float EffectiveRecoilMultiplier = 1.f;

private:
	float TimeSinceLastShot = 1000.f;
	float ConsecutiveShotDecayAccumulator = 0.f;

	float AccumulatedRecoilPitch = 0.f;
	float AccumulatedRecoilYaw = 0.f;

	// ADS 반동 전용 - 연속 사격 시작 후 경과 시간(초). RecoilCurve를 이 값으로 evaluate한다.
	float FiringElapsedTime = 0.f;
	// 직전 프레임에 evaluate한 RecoilCurve 값(X=Yaw, Y=Pitch, Pitch는 RecoilMaxPitch로 Clamp된 값).
	FVector2D PrevRecoilOffset = FVector2D::ZeroVector;
	// 지금 ADS 반동을 생성 중인지(사격 유지 구간). 새 버스트 시작 감지에 쓰인다. 탄퍼짐 블룸
	// 카운터(ConsecutiveShotCount)와는 별개로 관리해서, ADS 중 사격 후 지향 사격으로 전환해도
	// 그동안 쌓인 블룸이 갑자기 튀어나오지 않는다.
	bool bRecoilBurstActive = false;
};