// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponAimComponent.generated.h"

class UCurveVector;
class UDataTable;
struct FWeaponSpreadRecoilRow;

// ADS 상태 + 탄퍼짐(블룸) + 반동을 전담한다. 전투 시스템 명세서 3.3.1 원칙이 테스트 후 개정되어,
// 반동은 지향 사격/ADS 공용이며, 사격 블룸은 지향 사격 전용이다. ADS에도 이동/공중 페널티는 적용된다.
// 이 셋이 bIsADS를 기준으로 얽혀 있는 한 묶음의 개념이라 하나의 컴포넌트로 분리했다. AWeaponBase는 "언제" 갱신할지(Fire()/Tick() 호출 시점)만
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
	float GetADSFieldOfView() const;
	float GetADSCameraInterpSpeedOverride() const { return ADSCameraInterpSpeedOverride; }
	float GetADSMoveSpeedMultiplier() const { return ADSMoveSpeedMultiplier; }
	bool SupportsVariableADSZoom() const { return bEnableVariableADSZoom; }
	bool AdjustADSZoom(float InputDelta);
	int32 GetCurrentADSZoomStep() const { return CurrentADSZoomStep; }
	int32 GetADSZoomStepCount() const { return FMath::Max(2, ADSZoomStepCount); }
	bool ShouldHideFirstPersonPresentationWhileADS() const { return bHideFirstPersonPresentationWhileADS; }
	bool ShouldHideMainHUDWhileADS() const { return bHideMainHUDWhileADS; }

	// --- 탄퍼짐 ---
	void SetMovementSpreadMultiplier(float NewMultiplier);
	// 공중(점프/낙하) 여부. ADS 중엔 탄퍼짐이 0으로 고정되지만, 공중에서는 그 고정을 뚫고
	// InAirSpreadPenaltyDegrees만큼 탄퍼짐이 강제로 붙는다 - 조준해도 공중에서는 정확도가 떨어져야
	// 하기 때문(0 * 배율은 항상 0이라 곱셈만으로는 표현할 수 없어 덧셈 페널티로 분리했다).
	void SetIsAirborne(bool bNewIsAirborne);
	// 정지(또는 브레이킹 성공)로 취급되는 이동 상태인지. ADS 탄퍼짐이 0으로 고정되는 조건이
	// "ADS 중"에서 "ADS 중 + 정지(브레이킹 성공)"로 바뀌었다 - CombatComponent::
	// UpdateMovementSpreadMultiplier가 매 틱 갱신해서 넘겨준다.
	void SetIsStationary(bool bNewIsStationary);
	// 앉아 있는지. CombatComponent::UpdateMovementSpreadMultiplier가 매 틱 갱신한다.
	void SetIsCrouching(bool bNewIsCrouching);
	void SetEffectiveSpreadDegrees(float NewSpreadDegrees);
	float GetBaseSpreadDegrees() const { return BaseSpreadDegrees; }
	// 지금 이 순간 실제로 적용 중인 탄퍼짐 각도(도). ADS/연사 블룸/이동 배율/증강까지 전부 반영된 최종값.
	float GetCurrentSpreadDegrees() const;
	// 마지막 실제 지향 발사 이후의 게임 시간으로 평가한다. 틱 순서나 UI 갱신에 의존하지 않는다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Spread")
	float GetCurrentBloomDegrees() const;

	// --- 반동 ---
	void SetEffectiveRecoilMultiplier(float NewMultiplier);
	// RecoilCurve의 Yaw(X채널) 결과에 곱해지는 배율을 강제로 덮어쓴다. 0을 넘기면 이 무기는 좌우
	// 반동이 완전히 사라진다(예: 스나이퍼). AWeaponBase::BeginPlay가 무기 태그를 보고 호출한다.
	void SetRecoilYawMultiplier(float NewMultiplier);

	// AWeaponBase::Tick()에서 매 프레임 호출한다. 블룸 읽기값과 반동 생성/회복을 갱신한다.
	void TickAim(float DeltaTime);
	// 실제 탄 판정이 끝난 성공한 발사만 알린다. 블룸 증가는 다음 탄부터 반영된다.
	// ShotInterval은 그 발사에 사용한 실효 RPM의 간격이다. 입력 해제로 초기화하지 않는다.
	void NotifyShotFired(float ShotInterval = 0.f);
	// GA_Reload 진입/무기 교체 등 AWeaponBase가 반동을 즉시 0으로 되돌려야 할 때 호출한다.
	void ResetRecoilState();

protected:
	virtual void BeginPlay() override;

	// SpreadRecoilTable + SpreadRecoilRowName이 지정된 경우, 해당 행의 값으로 아래 탄퍼짐/반동
	// 튜닝 값들을 덮어쓴다. BeginPlay에서 호출된다.
	void ApplySpreadRecoilTuningTable();
	void ApplySpreadRecoilTuning(const FWeaponSpreadRecoilRow& Tuning);

	float CalculateCurrentSpreadDegrees() const;
	void UpdateLegacyShotCount();

	// 사격 유지 구간(TimeSinceLastShot < RecoilRecoveryDelay)에 매 프레임 호출된다(ADS 여부 무관 - 지향
	// 사격/ADS 공용). RecoilCurve(UCurveVector, X=Yaw/Y=Pitch)를 FiringElapsedTime으로 계속 evaluate해
	// 이전 프레임과의 델타만 컨트롤러에 적용한다 - 발당 1회가 아니라 시간 기반 연속 적용이라 프레임/RPM과
	// 무관하게 항상 같은 반동 패턴을 그린다.
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

	// ADS(조준) 중 이동속도 배율. 1이면 영향 없음. MoveSpeedMultiplier(무기를 들고만 있어도 항상 적용)와는
	// 별개로, ADS 중일 때만 추가로 곱해진다 - ShooterMovementComponent::GetMaxSpeed 참고. 무기마다
	// 다르게 설정 가능(예: 스나이퍼는 라이플보다 스코프 중 더 많이 느려지게).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ADSMoveSpeedMultiplier = 0.5f;

	// 이 무기만의 FOV 보간 속도 오버라이드. 0이면 CombatComponent의 ADSZoomInterpSpeed를 사용한다.
	// 스나이퍼처럼 다른 무기보다 ADS 진입/해제와 줌 단계 전환을 느리게 만들고 싶을 때 설정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS", meta = (ClampMin = "0.0"))
	float ADSCameraInterpSpeedOverride = 0.f;

	// 켜면 고정 ADS FOV 대신 마우스 휠로 단계형 줌을 조절한다. 현재 단계는 ADS를 해제해도
	// 리셋하지 않으므로 같은 무기로 다시 ADS했을 때 마지막 줌 단계가 그대로 유지된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS|Variable Zoom")
	bool bEnableVariableADSZoom = false;

	// 최소 배율(가장 넓게 보이는 상태)의 FOV. 값이 클수록 덜 확대된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS|Variable Zoom",
		meta = (EditCondition = "bEnableVariableADSZoom", ClampMin = "5.0", ClampMax = "170.0"))
	float MinimumMagnificationFieldOfView = 40.f;

	// 최대 배율(가장 좁게 보이는 상태)의 FOV. 값이 작을수록 더 확대된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS|Variable Zoom",
		meta = (EditCondition = "bEnableVariableADSZoom", ClampMin = "5.0", ClampMax = "170.0"))
	float MaximumMagnificationFieldOfView = 15.f;

	// 최소~최대 배율 사이의 단계 수. 0번 단계가 최소 배율, 마지막 단계가 최대 배율이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS|Variable Zoom",
		meta = (EditCondition = "bEnableVariableADSZoom", ClampMin = "2", UIMin = "2", UIMax = "10"))
	int32 ADSZoomStepCount = 4;

	// 무기가 처음 생성됐을 때 시작할 줌 단계. 이후 ADS On/Off로는 변경되지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS|Variable Zoom",
		meta = (EditCondition = "bEnableVariableADSZoom", ClampMin = "0"))
	int32 DefaultADSZoomStep = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS|Variable Zoom")
	int32 CurrentADSZoomStep = 0;

	// 스코프처럼 ADS 중 1인칭 팔/무기 표현을 가려야 하는 무기만 true로 설정한다. 실제 표시 전환은
	// CombatComponent가 로컬 플레이어의 Mesh1P에만 적용하며, 다른 플레이어가 보는 Mesh3P에는 영향이 없다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS")
	bool bHideFirstPersonPresentationWhileADS = false;

	// 스코프 ADS처럼 일반 인게임 HUD를 가려야 하는 무기만 true로 설정한다. HUD가 무기 태그를
	// 직접 분기하지 않도록 무기별 ADS 표현 정책은 Aim 컴포넌트가 소유한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS")
	bool bHideMainHUDWhileADS = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|ADS")
	bool bIsADS = false;

	// --- 탄퍼짐 튜닝 값 ---
	// DT와 공통인 기본값은 생성자에서 FWeaponSpreadRecoilRow로 초기화한다.
	// 무기 기본 탄퍼짐 각도. SpreadRecoilTable 미지정 시 사용되는 기본값(=fallback).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float BaseSpreadDegrees;

	// 현재 적용 중인 실제 기본 탄퍼짐 각도. SetEffectiveSpreadDegrees로만 갱신되며, 기본값은
	// (SpreadRecoilTable 적용 이후의) BaseSpreadDegrees와 동일하게 초기화된다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float EffectiveBaseSpreadDegrees;

	// 연속 사격 시 사격당 누적되는 탄퍼짐 각도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float BloomPerShotDegrees;

	// 연사 블룸 누적 최대치
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float MaxBloomDegrees;

	// 실제 발사 간격이 끝난 뒤 블룸 회복까지의 여유 시간(초). 연사 틱의 작은 지연을 흡수한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread", meta = (ClampMin = "0.0", Units = "s"))
	float BloomRecoveryDelay;

	// 최대 블룸에서 0까지 걸리는 회복 시간(초). 작은 블룸은 같은 속도로 더 빨리 회복한다.
	// 0이면 위 대기 시간이 지난 시점에 사격 블룸만 즉시 0이 된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread", meta = (ClampMin = "0.0", Units = "s"))
	float BloomRecoveryTime;

	// 기존 BP/DT 직렬화 호환용. 새 회복 계산에는 사용하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use BloomRecoveryDelay; measured after the shot interval."))
	float SpreadDecayDelay = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use BloomRecoveryTime; seconds from maximum bloom to zero."))
	float ConsecutiveShotDecayPerSecond = 5.f;

	// 이동 상태 탄퍼짐 배율. SetMovementSpreadMultiplier로만 갱신된다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float MovementSpreadMultiplier = 1.f;

	// 공중일 때 ADS 여부와 무관하게 추가로 붙는 탄퍼짐(도). SetIsAirborne로만 갱신되는
	// bIsAirborne이 true일 때 CalculateCurrentSpreadDegrees에 더해진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float InAirSpreadPenaltyDegrees = 3.f;

	// 정지 상태가 아닐 때(걷기/스프린트, 공중 포함) ADS 여부와 무관하게 추가로 붙는 탄퍼짐(도).
	// InAirSpreadPenaltyDegrees와 동일한 방식 - MovementSpreadMultiplier(배율)만으로는 기본
	// 탄퍼짐이 작은 무기에서 체감이 약할 수 있어서, 배율과 별개로 무조건 더해지는 고정값을 둔다.
	// 기본값 0이면 기존과 동일하게 동작한다(하위 호환).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float MovingSpreadPenaltyDegrees = 0.f;

	// SetIsAirborne으로만 갱신된다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	bool bIsAirborne = false;

	// ADS 중 이동으로 브레이킹(정지)에 실패했을 때 적용되는 탄퍼짐(도). SpreadRecoilTable 미지정 시
	// 사용되는 기본값(=fallback).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float MovingADSSpreadDegrees;

	// SetIsStationary로만 갱신된다. true면 ADS 탄퍼짐이 0으로 고정(브레이킹 성공), false면
	// MovingADSSpreadDegrees가 적용된다(브레이킹 실패, 이동 중 조준).
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	bool bIsStationary = true;

	// 앉아서 쏠 때 탄퍼짐에 곱해지는 배율(1=영향 없음). 공중 페널티에는 적용되지 않는다.
	// SpreadRecoilTable 미지정 시 사용되는 기본값(=fallback).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrouchSpreadMultiplier;

	// SetIsCrouching으로만 갱신된다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	bool bIsCrouching = false;

	// 기존 BP 읽기 호환용: 남은 블룸의 발수 환산값(올림). 실제 발사 횟수/탄퍼짐 원본이 아니다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Spread|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use GetCurrentBloomDegrees for continuous bloom."))
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
	float RecoilRandomness;

	// 누적 가능한 최대 상방 반동(도). 초과분은 Clamp - 연사 중 반동이 무한정 커지지 않고 이 이상은
	// 좌우 흔들림(RecoilRandomness)만 남는 상한선 역할을 한다. Yaw에는 상한이 없다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilMaxPitch;

	// RecoilCurve의 Yaw(X채널) 결과에 곱해지는 배율. 0으로 두면 이 무기는 좌우 반동이 전혀 없고
	// Pitch(상하) 반동만 남는다 - 예: 스나이퍼는 X축 반동을 없애고 싶을 때 0으로 설정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilYawMultiplier;

	// 사격 중단 후 이 시간(초)이 지나야 반동 복귀가 시작된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilRecoveryDelay;

	// 반동 복귀 FInterpTo 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilRecoverySpeed;

	// 현재 적용 중인 실제 반동 배율. SetEffectiveRecoilMultiplier로만 갱신되며, RecoilCurve 샘플링
	// 값에 곱해진다. 기본값 1이면 배율 없음(원본 커브 그대로).
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float EffectiveRecoilMultiplier = 1.f;

private:
	// 반동 전용 시계. 블룸 회복은 별도의 게임 시간으로 평가한다.
	float TimeSinceLastShot = 1000.f;

	// 지향 사격 직후의 제한된 블룸과 회복 시작 시각. 최대치를 넘는 누적은 저장하지 않는다.
	float BloomAtLastShot = 0.f;
	double BloomRecoveryStartTime = 0.0;

	float AccumulatedRecoilPitch = 0.f;
	float AccumulatedRecoilYaw = 0.f;

	// ADS 반동 전용 - 연속 사격 시작 후 경과 시간(초). RecoilCurve를 이 값으로 evaluate한다.
	float FiringElapsedTime = 0.f;
	// 직전 프레임에 evaluate한 RecoilCurve 값(X=Yaw, Y=Pitch, Pitch는 RecoilMaxPitch로 Clamp된 값).
	FVector2D PrevRecoilOffset = FVector2D::ZeroVector;
	// 지금 반동을 생성 중인지(사격 유지 구간). 새 버스트 시작 감지에 쓰며 블룸과 독립적이다.
	bool bRecoilBurstActive = false;
};
