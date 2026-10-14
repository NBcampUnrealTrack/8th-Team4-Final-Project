// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "PlayerInterface.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class AWeaponBase;
class UCombatComponent;

UINTERFACE()
class UPlayerInterface : public UInterface
{
	GENERATED_BODY()
};

class FPS_API IPlayerInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FPS|Player")
	USkeletalMeshComponent* GetMesh1P() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FPS|Player")
	USkeletalMeshComponent* GetMesh3P() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FPS|Player")
	UCameraComponent* GetViewCamera() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FPS|Player")
	FRotator GetFixedAimRotation() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FPS|Player")
	AWeaponBase* GetCurrentWeapon() const;

	// UI가 CombatComponent의 OnCurrentWeaponChanged 등 델리게이트를 구독하려고 참조가 필요할 때 쓴다.
	// GetCurrentWeapon()과 달리 매 프레임 폴링이 아니라 최초 바인딩 시 한 번만 호출하는 용도.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FPS|Player")
	UCombatComponent* GetCombatComponent() const;

	// 무기가 생성되거나 오너가 확정됐을 때 무기가 호출
	UFUNCTION(BlueprintNativeEvent, Category = "FPS|Player")
	void OnWeaponSpawned(AWeaponBase* Weapon);
};
