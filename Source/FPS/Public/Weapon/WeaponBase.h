// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "Weapon/WeaponTypes.h"
#include "WeaponBase.generated.h"

class UCameraComponent;
class UCurveFloat;
struct FHitResult;

// 무기 사격 파이프라인 담당 클래스.
// GAS(GameplayAbilitySystem)의 존재를 전혀 모른다 - AbilitySystemComponent, GameplayTag,
// GameplayEffect 등 GAS 관련 타입/include를 절대 사용하지 않는다.
// EffectiveRPM처럼 GAS 쪽 영향이 필요한 값은 외부(GA_RangedAttack)가 계산해서 Setter로 꽂아준다.
UCLASS()
class FPS_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	virtual void Tick(float DeltaTime) override;

	// 다른 팀원의 GAS Adapter가 구독해서 FRogueDamageRequest 등으로 변환한다.
	UPROPERTY(BlueprintAssignable, Category = "FPS|Weapon")
	FOnWeaponHit OnWeaponHit;

	// RPM 쿨다운 확인 -> 재장전/탄약 확인 -> 탄퍼짐 반영 라인트레이스 -> 히트 브로드캐스트 -> 반동 적용 -> 연사 카운트 증가.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	bool Fire();

	// 재장전 상태 머신 진입 (Idle -> Reloading). 재장전 중에는 Fire()가 차단된다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void StartReload();

	// ADS 진입/해제. 탄퍼짐(0으로 고정)과 반동 적용 여부에 영향을 준다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetADS(bool bNewIsADS);

	// 현재 적용 중인 실제 발사속도(RPM)를 갱신한다.
	// 이 값을 누가 계산해서 넣어주는지(GAS 스탯/증강 등)는 AWeaponBase가 알 필요가 없다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetEffectiveRPM(float NewRPM);

	// 이동 상태에 따른 탄퍼짐 배율(정지 1.0 / 걷기 1.5 / 공중,뛰기 2.5 / 브레이킹 성공 1.0)을 외부에서 갱신한다.
	// AWeaponBase는 이동 컴포넌트를 직접 참조하지 않으므로, 호출자(Character/CombatComponent)가 계산해서 넣어준다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetMovementSpreadMultiplier(float NewMultiplier);

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool CanFireNow() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetBaseRPM() const { return BaseRPM; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetEffectiveRPM() const { return EffectiveRPM; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool IsADS() const { return bIsADS; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

protected:
	virtual void BeginPlay() override;

	// --- 탄퍼짐 ---
	float CalculateCurrentSpreadDegrees() const;
	void TickSpreadDecay(float DeltaTime);

	// --- 반동 ---
	void ApplyProceduralRecoil();
	void TickRecoilRecovery(float DeltaTime);

	// --- 관통 라인트레이스 ---
	bool GetTraceOriginAndDirection(FVector& OutStart, FVector& OutDirection) const;
	void PerformPenetratingLineTrace(const FVector& TraceStart, const FVector& TraceDirection, int32 PelletIndex);
	float GetSurfaceThickness(const FHitResult& Hit) const;
	float GetSurfaceHardness(const FHitResult& Hit) const;
	static float CalculateHitAngleDegrees(const FHitResult& Hit, const FVector& TraceDirection);

	// --- 재장전 ---
	void FinishReload();

protected:
	// 발당 기본 대미지 (관통 감쇠 이전 값)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	float BaseDamage = 26.f;

	// 무기 자체의 고정 발사속도. 증강/스탯 보정 이전 값.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	float BaseRPM = 600.f;

	// 현재 적용 중인 실제 발사속도. SetEffectiveRPM으로만 갱신되며, 기본값은 BaseRPM과 동일하게 초기화된다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	float EffectiveRPM = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	int32 MagazineSize = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	float ReloadDuration = 2.1f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	int32 CurrentAmmo = 30;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	bool bIsReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|ADS")
	bool bIsADS = false;

	// 연속 사격 횟수. 탄퍼짐 블룸과 반동 커브 샘플링 기준 시간 계산에 사용.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	int32 ConsecutiveShotCount = 0;

	// --- 탄퍼짐 튜닝 값 ---
	// 무기 기본 탄퍼짐 각도. 임의로 1 설정, 추후 DataTable(FWeaponDataRow)로 이관 예정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float BaseSpreadDegrees = 1.f;

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

	// --- 반동 튜닝 값 ---
	// 피치(위/아래) 반동 커브. 발사 시점부터의 경과 시간을 기준으로 샘플링한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	TObjectPtr<UCurveFloat> RecoilCurve;

	// RecoilCurve 값에 곱해 요(좌우) 반동을 만드는 계수. 0이면 순수 상방 반동만 발생.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilYawFactor = 0.f;

	// 사격 중단 후 이 시간(초)이 지나야 반동 복귀가 시작된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilRecoveryDelay = 0.15f;

	// 반동 복귀 FInterpTo 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilRecoverySpeed = 10.f;

	// --- 관통 튜닝 값 ---
	// 무기 기본 관통력. 벽 두께 대비 남은 관통력 판정 및 데미지 감쇠 계산에 사용.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float PenetrationPower = 70.f;

	// 라인트레이스 최대 사거리 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float TraceRange = 4000.f;

	// UPenetrableSurfaceComponent가 없는 액터에 적용할 기본 관통 두께
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float DefaultSurfaceThickness = 10.f;

	// SurfaceHardnessMap에 없는 표면 타입에 적용할 기본 경도(관통 난이도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float DefaultSurfaceHardness = 0.f;

	// 표면 타입(EPhysicalSurface)별 경도. 남은 관통력이 이 값보다 커야 관통 가능.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	TMap<TEnumAsByte<EPhysicalSurface>, float> SurfaceHardnessMap;

private:
	// 사격 위치/방향 계산에 사용하는 카메라. 소유 액터에서 BeginPlay 시 캐싱한다.
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedOwnerCamera;

	FTimerHandle ReloadTimerHandle;

	float LastFireTime = -1000.f;
	float TimeSinceLastShot = 1000.f;
	float BurstStartTime = 0.f;
	float ConsecutiveShotDecayAccumulator = 0.f;

	float AccumulatedRecoilPitch = 0.f;
	float AccumulatedRecoilYaw = 0.f;
};