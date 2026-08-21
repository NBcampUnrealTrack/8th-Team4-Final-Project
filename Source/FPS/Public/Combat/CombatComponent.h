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

private:
	AWeaponBase* SpawnWeaponInstance(TSubclassOf<AWeaponBase> WeaponClass);
	void ClearWeaponLoadout();
	void ApplyCurrentWeapon(AWeaponBase* NewWeapon);
	void RefreshWeaponPresentation();
	void BeginEquipCycle(AWeaponBase* Weapon);
	float ResolveEquipDuration(AWeaponBase* Weapon) const;
	void SetCyclingState(bool bNewIsCycling);

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
