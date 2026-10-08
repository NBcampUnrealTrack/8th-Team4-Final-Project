// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Weapon/WeaponTypes.h"

#include "CombatComponent.generated.h"

class AWeaponBase;
class UWeaponData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCyclingStateChanged, bool, bIsCycling);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMainHUDSuppressionChanged, bool, bSuppressed);
// 현재 손에 든 무기(CurrentWeapon)가 바뀔 때마다 브로드캐스트된다. UI(ShooterWeaponStatus)가 매 프레임
// GetCurrentWeapon()을 폴링하는 대신 이 델리게이트를 구독하도록 하기 위함이다. NewWeapon은 nullptr일 수 있다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentWeaponChanged, AWeaponBase*, NewWeapon);
// 크로스헤어 간격 계산에 쓰이는 값(장착 무기, 탄퍼짐 각도, 카메라 FOV) 중 하나라도 바뀌었을 때만 브로드캐스트된다.
// UShooterReticle이 NativeTick 폴링 대신 이 델리게이트를 구독한다.
DECLARE_MULTICAST_DELEGATE(FOnCrosshairSpreadInputsChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrosshairTargetChanged, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAimingStateChanged, bool);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();

	// 기존 단일 무기 장착 API. Debug/레거시 호출자를 위해 의미를 유지한다.
	// 호출 시 보관 중인 슬롯 무기를 정리하고 새 단일 무기를 생성한다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Combat")
	AWeaponBase* EquipWeapon(TSubclassOf<AWeaponBase> WeaponClass);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Combat")
	void UnequipWeapon();

	// 1/2/3번 슬롯(주무기/보조무기/근접무기)용. 무기를 한 번만 생성해 보관하므로 스왑 후에도
	// 탄약/런타임 상태가 유지된다. MeleeWeaponClass는 비워도 되며(예: 근접무기 없는 로드아웃),
	// 그 경우 Melee 슬롯은 빈 채로 남는다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Combat|Loadout")
	bool InitializeWeaponLoadout(
		TSubclassOf<AWeaponBase> PrimaryWeaponClass,
		TSubclassOf<AWeaponBase> SecondaryWeaponClass,
		TSubclassOf<AWeaponBase> MeleeWeaponClass);

	// 현재 보유 무기와 이후 생성·장착하는 무기에 무한 탄약 상태를 동일하게 전달한다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Combat|Loadout")
	void SetInfiniteAmmo(bool bNewInfiniteAmmo);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Combat|Loadout")
	bool SwitchWeaponSlot(EWeaponSlot WeaponSlot);

	/**
	 * 주무기 슬롯 한 칸만 교체하고 손에 든다. Secondary(권총)와 Melee(칼)는 건드리지 않는다.
	 * nullptr을 넘기면 주무기를 비우고 권총으로 내려간다.
	 * ClearWeaponLoadout()을 호출하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Combat|Loadout")
	bool ReplacePrimaryWeapon(TSubclassOf<AWeaponBase> PrimaryWeaponClass);

	// 현재 무기 재장착, 유틸리티 장착 후 복귀에서 호출
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Combat|Loadout")
	void ReEquipCurrentWeapon();

	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Loadout")
	AWeaponBase* GetWeaponInSlot(EWeaponSlot WeaponSlot) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Loadout")
	EWeaponSlot GetCurrentWeaponSlot() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	AWeaponBase* GetEquippedWeapon() const { return CurrentWeapon; }

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	AWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	FGameplayTag GetCurrentWeaponTag() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Combat")
	void SetCurrentWeapon(AWeaponBase* NewWeapon);

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	bool IsCycling() const { return bIsCycling; }

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	bool IsWeaponReady() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	UWeaponData* GetWeaponData() const { return WeaponData; }

	// 서버에서 스왑 종료 시 호출한다. WeaponSwitchDuration 타이머가 기본 경로이며,
	// 추후 서버 권위 AnimNotify를 붙여도 같은 API를 사용할 수 있다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	void FinishCycling();

	// 부활/빙의 직후처럼 무기 표현이 늦게 잡히는 경우, 외부(캐릭터의 PossessedBy/OnRep_PlayerState)에서
	// 부착·가시성을 강제로 다시 적용한다. RefreshWeaponPresentation()은 멱등이라 반복 호출해도 안전하다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	void RefreshEquippedWeaponPresentation() { RefreshWeaponPresentation(); }

	UPROPERTY(BlueprintAssignable, Category = "FPS|Combat")
	FOnCyclingStateChanged OnCyclingStateChanged;

	// 로컬 플레이어의 ADS 표현 정책이 Main HUD를 가려야 하는 상태로 바뀔 때 알린다.
	// UI는 무기 태그를 직접 해석하지 않고 이 상태만 구독한다.
	UPROPERTY(BlueprintAssignable, Category = "FPS|Combat|ADS")
	FOnMainHUDSuppressionChanged OnMainHUDSuppressionChanged;

	// 손에 든 무기가 바뀔 때(장착/스왑/해제) 알린다. UI는 이 델리게이트만 구독하면 되고 CombatComponent를
	// 매 프레임 폴링할 필요가 없다.
	UPROPERTY(BlueprintAssignable, Category = "FPS|Combat")
	FOnCurrentWeaponChanged OnCurrentWeaponChanged;

	// 구독자가 있을 때만(=로컬 크로스헤어 위젯이 붙은 Pawn) 매 틱 값 변화를 비교한다.
	FOnCrosshairSpreadInputsChanged OnCrosshairSpreadInputsChanged;

	// 로컬 중앙 조준점의 적 여부가 바뀔 때만 알린다. 서버 복제 상태가 아니다.
	FOnCrosshairTargetChanged OnCrosshairTargetChanged;
	bool IsAimingAtEnemy() const { return bAimingAtEnemy; }

	FOnAimingStateChanged OnAimingStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<UWeaponData> WeaponData;

	// 기존 BP 읽기 호환용 적용 상태. 서버 복제 값과 로컬 예측 결과를 분리한다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat")
	bool bAiming = false;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat|ADS")
	bool IsAiming() const;

	// ExpectedWeapon은 늦은 이전 능력 종료가 새 무기의 조준을 해제하지 않게 한다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	void SetAiming(bool bNewAiming, AWeaponBase* ExpectedWeapon = nullptr);

	// 사망/아바타 전환에서만 FOV까지 즉시 기본 상태로 복구한다.
	void ResetAiming();

	// 현재 ADS 상태와 장착 무기의 표현 설정을 조합한 최종 Main HUD 숨김 여부.
	UFUNCTION(BlueprintPure, Category = "FPS|Combat|ADS")
	bool ShouldSuppressMainHUDForADS() const;

	// 유틸리티 홀드 중 무기 표현 억제
	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	void SetPresentationSuppressed(bool bSuppressed);

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	bool IsPresentationSuppressed() const { return bPresentationSuppressed; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(Transient, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWeapon, Category = "FPS|Combat")
	TObjectPtr<AWeaponBase> CurrentWeapon;

	UPROPERTY(Transient, BlueprintReadOnly, ReplicatedUsing = OnRep_WeaponSlots, Category = "FPS|Combat|Loadout")
	TObjectPtr<AWeaponBase> PrimaryWeapon;

	UPROPERTY(Transient, BlueprintReadOnly, ReplicatedUsing = OnRep_WeaponSlots, Category = "FPS|Combat|Loadout")
	TObjectPtr<AWeaponBase> SecondaryWeapon;

	UPROPERTY(Transient, BlueprintReadOnly, ReplicatedUsing = OnRep_WeaponSlots, Category = "FPS|Combat|Loadout")
	TObjectPtr<AWeaponBase> MeleeWeapon;

	// 실제 Equip Montage가 연결되기 전에도 스왑 중 사격/재장전이 열리지 않도록 하는 서버 fallback 시간.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Loadout", meta = (ClampMin = "0.0"))
	float WeaponSwitchDuration = 0.35f;

	// --- 이동 상태 탄퍼짐 배율 ---
	// 이 속도(cm/s, 수평 기준) 이하면 정지로 취급한다. 브레이킹(감속)으로 이 속도 밑으로 떨어져도
	// 같은 취급이라 별도의 "브레이킹 성공" 판정을 두지 않는다 - 정지든 방금 멈췄든 결과는 같다.
	// 0이면 속도가 완전히 0일 때만 정지로 인정된다(미세한 잔여 속도도 이동 중으로 취급).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Spread", meta = (ClampMin = "0.0"))
	float StationarySpeedThreshold = 0.f;

	// 정지(또는 브레이킹 성공) 상태 탄퍼짐 배율.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Spread")
	float SpreadMultiplierStationary = 1.f;

	// 걷기 상태 탄퍼짐 배율.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Spread")
	float SpreadMultiplierWalking = 1.5f;

	// 공중/스프린트 상태 탄퍼짐 배율.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Spread")
	float SpreadMultiplierAirOrSprint = 2.5f;

	// --- ADS 카메라 줌 ---
	// ADS 진입/해제와 가변 줌 단계 전환에 공통으로 쓰는 카메라 FOV 보간 속도(FInterpTo).
	// 캐릭터 BP의 CombatComponent에서 조정할 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|ADS", meta = (ClampMin = "0.0"))
	float ADSZoomInterpSpeed = 10.f;

private:
	// 서버의 로드아웃 설정이다. 클라이언트에는 각 무기의 bInfiniteAmmo가 복제된다.
	bool bInfiniteAmmo = false;

	UPROPERTY(ReplicatedUsing = OnRep_Aiming)
	bool bReplicatedAiming = false;

	TWeakObjectPtr<AWeaponBase> AppliedAimingWeapon;
	bool bLastBroadcastAiming = false;
	bool UsesLocalAimingPrediction() const;
	void ApplyAimingPresentation();
	void CancelAimingForWeapon(AWeaponBase* Weapon);

	AWeaponBase* SpawnWeaponInstance(TSubclassOf<AWeaponBase> WeaponClass);
	void ClearWeaponLoadout();
	void ApplyCurrentWeapon(AWeaponBase* NewWeapon);
	void RefreshWeaponPresentation();
	void BeginEquipCycle(AWeaponBase* Weapon);
	float ResolveEquipDuration(AWeaponBase* Weapon) const;
	void SetCyclingState(bool bNewIsCycling);
	// 매 틱 장착 무기의 SetMovementSpreadMultiplier를 이동 상태(정지/걷기/뛰기/공중) 기준으로 갱신한다.
	void UpdateMovementSpreadMultiplier() const;
	// 매 틱 카메라 FOV를 평상시 값 <-> 조준 시 목표값 사이로 보간한다.
	void UpdateCameraZoom(float DeltaTime) const;
	// 현재 ADS/무기 설정에 맞춰 로컬 플레이어의 1인칭 팔/무기 표시만 갱신한다.
	void RefreshFirstPersonADSPresentation() const;
	// 현재 ADS/무기 설정에 맞춰 로컬 Main HUD suppression 상태를 갱신하고 변경 시 알린다.
	void RefreshMainHUDSuppressionState();
	// 크로스헤어 입력값(무기/탄퍼짐/FOV)이 직전 브로드캐스트 이후 바뀌었으면 OnCrosshairSpreadInputsChanged를 알린다.
	void BroadcastCrosshairSpreadInputsIfChanged();
	void UpdateCrosshairTarget();
	void SetAimingAtEnemy(bool bNewAimingAtEnemy);
	bool bAimingAtEnemy = false;

	// 마지막으로 OnCrosshairSpreadInputsChanged에 실어 보낸 값. 같은 값의 중복 브로드캐스트를 막는다.
	TWeakObjectPtr<AWeaponBase> LastCrosshairWeapon;
	float LastCrosshairSpreadDegrees = -1.f;
	float LastCrosshairFieldOfView = -1.f;

	// BeginPlay 시점에 캐싱해두는 오너 카메라의 평상시(비-ADS) FOV. 무기별 ADS FOV와 보간할 때
	// 기준값으로 쓴다 - 캐릭터 BP마다 카메라 기본 FOV가 다를 수 있어 하드코딩하지 않는다.
	UPROPERTY(Transient)
	float DefaultFieldOfView = 90.f;
	bool bDefaultFieldOfViewInitialized = false;

	// UI에 마지막으로 전달한 로컬 ADS HUD suppression 상태. 같은 값의 중복 브로드캐스트를 막는다.
	bool bMainHUDSuppressedForADS = false;

	// 유틸리티 홀드 중 무기 표현 억제
	bool bPresentationSuppressed = false;

	UFUNCTION()
	void OnRep_CurrentWeapon(AWeaponBase* LastWeapon);

	UFUNCTION()
	void OnRep_Aiming();

	UFUNCTION()
	void OnRep_WeaponSlots();

	UFUNCTION()
	void OnRep_IsCycling();

	UPROPERTY(ReplicatedUsing = OnRep_IsCycling)
	bool bIsCycling = false;

	FTimerHandle WeaponSwitchTimerHandle;
};
