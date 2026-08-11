// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CombatComponent.generated.h"

class AWeaponBase;
class UWeaponData;

// CombatComponent의 Cycling(무기 교체) 상태가 바뀔 때 알리는 델리게이트. CombatComponent는 GAS를
// 전혀 모르므로(WeaponBase와 동일한 원칙) 태그를 직접 붙이지 않는다 - 이 델리게이트를 구독하는
// GAS Adapter 쪽이 bIsCycling==true인 동안 사격 어빌리티를 막는 태그를 붙였다 떼는 걸 책임진다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCyclingStateChanged, bool, bIsCycling);

// 무기 장착/해제(스폰, 오너 캐릭터의 Mesh1P/Mesh3P 소켓에 부착)를 담당한다.
// GAS 어빌리티 부여, 인풋 바인딩, 발사/재장전 로직은 다루지 않는다 - 그건 각각 GAS 쪽/무기 GA의 책임.
//
// 데디케이티드 서버 대상: EquipWeapon/UnequipWeapon은 반드시 오너(서버 권위)에서만 실행되어야 하고
// (HasAuthority() 체크), 실제 부착은 리슨 서버 자신은 즉시, 원격 클라이언트는 EquippedWeapon의
// OnRep을 통해 각자 로컬에서 AttachWeaponMeshesToOwner를 실행하는 방식으로 동기화한다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();

	// WeaponClass를 스폰해서 장착한다. 이미 장착 중인 무기가 있으면 먼저 해제(Destroy)한다.
	// 오너 액터가 IPlayerInterface(GetMesh1P/GetMesh3P)를 구현하지 않으면 스폰만 하고 부착은 하지 않는다.
	// 서버(권위)에서만 실행된다 - 클라이언트에서 호출되면 무시된다.
	// 호출 즉시 Cycling 상태로 들어간다(OnCyclingStateChanged(true) 브로드캐스트) - 애니메이션이
	// 끝나는 시점(Anim Notify 등)에 FinishCycling()을 호출해서 해제해야 한다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	AWeaponBase* EquipWeapon(TSubclassOf<AWeaponBase> WeaponClass);

	// 현재 장착 무기를 부착 해제하고 제거한다. 장착 무기가 없으면 아무 동작도 하지 않는다.
	// 서버(권위)에서만 실행된다 - 클라이언트에서 호출되면 무시된다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	void UnequipWeapon();

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	AWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

	// 무기 교체(Cycling) 진행 중인지. true인 동안 GAS 쪽에서 사격 어빌리티를 태그로 막는 데 쓴다.
	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	bool IsCycling() const { return bIsCycling; }

	// Cycling 상태를 해제한다. 무기 교체 애니메이션이 끝나는 시점(Anim Notify 등)에서 호출해서
	// 템포를 애니메이션 쪽에서 직접 조절할 수 있게 한다. Cycling 중이 아니면 아무 동작도 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	void FinishCycling();

	// GAS Adapter가 구독해서 Cycling 시작/종료에 맞춰 사격 차단 태그를 붙였다 뗀다.
	UPROPERTY(BlueprintAssignable, Category = "FPS|Combat")
	FOnCyclingStateChanged OnCyclingStateChanged;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	AWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	void SetCurrentWeapon(AWeaponBase* NewWeapon);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<UWeaponData> WeaponData;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "FPS|Combat")
	bool bAiming = false;

protected:
	virtual void BeginPlay() override;

	// 오너가 파괴되는 등으로 컴포넌트가 종료될 때, 부착만 해제되고 액터로 남는(고아가 되는) 것을
	// 막기 위해 장착 무기를 정리한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(Transient, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AWeaponBase> CurrentWeapon;

private:
	UFUNCTION()
	void OnRep_CurrentWeapon(AWeaponBase* LastWeapon);

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// Weapon의 Mesh1P/Mesh3P를 오너 캐릭터(IPlayerInterface)의 Mesh1P/Mesh3P에서
	// Weapon이 지정한 그립 소켓(GetGripSocketName1P/3P)으로 재부착한다.
	void AttachWeaponMeshesToOwner(AWeaponBase* Weapon) const;

	// 클라이언트에서 EquippedWeapon 리플리케이션이 도착했을 때 호출된다. 서버(권위) 자신은
	// EquipWeapon 안에서 직접 AttachWeaponMeshesToOwner를 부르므로 여기서 다시 부르지 않는다.
	UFUNCTION()
	void OnRep_EquippedWeapon(AWeaponBase* PreviousWeapon);

	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon, Transient)
	TObjectPtr<AWeaponBase> EquippedWeapon;

	// 서버(EquipWeapon 호출부)에서만 갱신된다 - 리플리케이트하지 않는다. 사격 차단은 GAS
	// ASC의 태그가 자체적으로 리플리케이트하므로, 여기서 또 리플리케이트할 필요는 없다.
	bool bIsCycling = false;
};
