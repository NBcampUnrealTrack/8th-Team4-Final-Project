// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Interfaces/PlayerInterface.h"
#include "DebugCombatTester.generated.h"

class AWeaponBase;
class UCameraComponent;
class UCombatComponent;
class USkeletalMeshComponent;

// 임시 테스트용 액터. AShooterCharacter에 CombatComponent가 정식으로 붙기 전에
// CombatComponent의 EquipWeapon/UnequipWeapon(스폰 + Mesh1P/Mesh3P 소켓 부착)만
// 단독으로 검증하기 위한 것. ShooterCharacter.h/.cpp를 건드리지 않기 위해
// IPlayerInterface(GetMesh1P/GetMesh3P)를 이 액터가 직접 구현한다.
//
// TODO: ShooterCharacter에 CombatComponent가 정식으로 붙으면 이 액터는 레벨에서 지우고
// 이 클래스 자체도 삭제할 것. 정식 파이프라인용 코드가 아니다.
UCLASS()
class FPS_API ADebugCombatTester : public AActor, public IPlayerInterface
{
	GENERATED_BODY()

public:
	ADebugCombatTester();

	virtual USkeletalMeshComponent* GetMesh1P_Implementation() const override;
	virtual USkeletalMeshComponent* GetMesh3P_Implementation() const override;
	virtual UCameraComponent* GetViewCamera_Implementation() const override;
	virtual FRotator GetFixedAimRotation_Implementation() const override;

protected:
	virtual void BeginPlay() override;

	// 캐릭터의 Mesh1P(팔) 역할. 에디터에서 SK_Mannequin_Arms 지정할 것.
	UPROPERTY(VisibleAnywhere, Category = "Debug|Mesh")
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	// 캐릭터의 Mesh3P(몸통) 역할. 에디터에서 SK_Mannequin 지정할 것.
	UPROPERTY(VisibleAnywhere, Category = "Debug|Mesh")
	TObjectPtr<USkeletalMeshComponent> Mesh3P;

	UPROPERTY(VisibleAnywhere, Category = "Debug|Combat")
	TObjectPtr<UCombatComponent> CombatComponent;

	// 테스트할 무기 블루프린트 (예: BP_Weapon_Pistol). BeginPlay에서 자동으로 장착된다.
	UPROPERTY(EditAnywhere, Category = "Debug|Weapon")
	TSubclassOf<AWeaponBase> WeaponClassToTest;

	// 이 키를 누르면 Unequip 후 WeaponClassToTest를 다시 Equip한다.
	// 부착 위치/소켓 보정을 PIE 재시작 없이 반복 확인하기 위한 것.
	UPROPERTY(EditAnywhere, Category = "Debug|Input")
	FKey RequipKey = EKeys::T;

private:
	void EquipTestWeapon();
	void HandleRequipPressed();
};
