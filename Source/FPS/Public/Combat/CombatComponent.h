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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Combat|Loadout")
	bool SwitchWeaponSlot(EWeaponSlot WeaponSlot);

	/**
	 * 주무기 슬롯 한 칸만 교체하고 손에 든다. Secondary(권총)와 Melee(칼)는 건드리지 않는다.
	 * nullptr을 넘기면 주무기를 비우고 권총으로 내려간다.
	 * ClearWeaponLoadout()을 호출하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Combat|Loadout")
	bool ReplacePrimaryWeapon(TSubclassOf<AWeaponBase> PrimaryWeaponClass);

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

	UPROPERTY(BlueprintAssignable, Category = "FPS|Combat")
	FOnCyclingStateChanged OnCyclingStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<UWeaponData> WeaponData;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "FPS|Combat")
	bool bAiming = false;

	// GA_ADS가 로컬 예측으로 호출한다(WeaponBase::SetEffectiveRPM과 같은 패턴 - 서버/클라 양쪽에서
	// 그대로 호출되고, 서버 값이 Replicated로 최종 동기화된다). ShooterAnimInstance가 bAiming을 읽어
	// 조준 블렌드에 쓴다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	void SetAiming(bool bNewAiming);

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Spread", meta = (ClampMin = "0.0"))
	float StationarySpeedThreshold = 10.f;

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
	// 평상시 FOV <-> 장착 무기의 AWeaponBase::ADSFieldOfView 사이를 보간하는 속도(FInterpTo).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|ADS")
	float ADSZoomInterpSpeed = 10.f;

private:
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

	// BeginPlay 시점에 캐싱해두는 오너 카메라의 평상시(비-ADS) FOV. 무기별 ADS FOV와 보간할 때
	// 기준값으로 쓴다 - 캐릭터 BP마다 카메라 기본 FOV가 다를 수 있어 하드코딩하지 않는다.
	UPROPERTY(Transient)
	float DefaultFieldOfView = 90.f;

	UFUNCTION()
	void OnRep_CurrentWeapon(AWeaponBase* LastWeapon);

	UFUNCTION()
	void OnRep_WeaponSlots();

	UFUNCTION()
	void OnRep_IsCycling();

	UPROPERTY(ReplicatedUsing = OnRep_IsCycling)
	bool bIsCycling = false;

	FTimerHandle WeaponSwitchTimerHandle;
};
