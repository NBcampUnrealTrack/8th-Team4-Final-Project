// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "DebugWeaponTester.generated.h"

class AWeaponBase;

// 임시 테스트용 액터. GA_RangedAttack/GAS 어빌리티 부여/인풋 바인딩이 아직 준비되기 전에
// AWeaponBase(탄퍼짐/반동/관통)만 단독으로 검증하기 위한 것.
// GAS를 완전히 건너뛰고 키 입력에서 바로 AWeaponBase 함수를 호출한다.
//
// TODO: GA_RangedAttack/GA_Reload + 정식 인풋 바인딩이 준비되면 이 액터는 레벨에서 지우고
// 이 클래스 자체도 삭제할 것. 정식 파이프라인용 코드가 아니다.
UCLASS()
class FPS_API ADebugWeaponTester : public AActor
{
	GENERATED_BODY()

public:
	ADebugWeaponTester();

protected:
	virtual void BeginPlay() override;

	// 테스트할 무기 블루프린트 (예: BP_Weapon_AR)
	UPROPERTY(EditAnywhere, Category = "Debug|Weapon")
	TSubclassOf<AWeaponBase> WeaponClassToTest;

	UPROPERTY(EditAnywhere, Category = "Debug|Input")
	FKey FireKey = EKeys::LeftMouseButton;

	// 칼처럼 bHeavyAttack(강공격) 쿨다운을 따로 테스트해야 하는 무기용. Fire(true)를 호출한다.
	// ADSKey(우클릭)와 겹치지 않게 별도 키로 뒀다 - bCanADS가 없는 무기는 ADSKey를 눌러도 어차피 무시된다.
	UPROPERTY(EditAnywhere, Category = "Debug|Input")
	FKey HeavyFireKey = EKeys::MiddleMouseButton;

	UPROPERTY(EditAnywhere, Category = "Debug|Input")
	FKey ADSKey = EKeys::RightMouseButton;

	UPROPERTY(EditAnywhere, Category = "Debug|Input")
	FKey ReloadKey = EKeys::R;

private:
	void HandleFirePressed();
	void HandleHeavyFirePressed();
	void HandleADSPressed();
	void HandleADSReleased();
	void HandleReloadPressed();

	UPROPERTY(Transient)
	TObjectPtr<AWeaponBase> TestWeapon;
};