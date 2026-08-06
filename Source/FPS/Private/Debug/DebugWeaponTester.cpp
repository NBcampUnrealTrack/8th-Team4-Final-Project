// Fill out your copyright notice in the Description page of Project Settings.

#include "Debug/DebugWeaponTester.h"

#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Weapon/WeaponBase.h"

namespace
{
	void ShowDebugMessage(const FString& Message, const FColor& Color)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugWeaponTester] %s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, Color, FString::Printf(TEXT("[DebugWeaponTester] %s"), *Message));
		}
	}
}

ADebugWeaponTester::ADebugWeaponTester()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADebugWeaponTester::BeginPlay()
{
	Super::BeginPlay();

	if (!WeaponClassToTest)
	{
		ShowDebugMessage(TEXT("WeaponClassToTest가 비어있음 - 배치한 액터의 Details에서 지정할 것"), FColor::Red);
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !PlayerPawn)
	{
		ShowDebugMessage(TEXT("PlayerController/Pawn을 못 찾음 - PIE로 실행 중인지 확인할 것"), FColor::Red);
		return;
	}

	// SpawnParams.Owner로 넘겨야 AWeaponBase::BeginPlay()가 카메라를 찾는 시점(스폰 직후)에
	// 이미 Owner가 세팅돼 있다. SpawnActor 이후에 SetOwner()를 부르면 이미 BeginPlay가 끝난 뒤라 늦는다.
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PlayerPawn;

	TestWeapon = GetWorld()->SpawnActor<AWeaponBase>(WeaponClassToTest, PlayerPawn->GetActorTransform(), SpawnParams);
	if (!TestWeapon)
	{
		ShowDebugMessage(TEXT("무기 스폰 실패"), FColor::Red);
		return;
	}

	// GA_RangedAttack::GetEquippedWeapon()이 나중에 Attach된 액터를 탐색하는 방식과 동일하게 맞춰둔다.
	TestWeapon->AttachToActor(PlayerPawn, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	EnableInput(PC);
	if (!InputComponent)
	{
		ShowDebugMessage(TEXT("InputComponent 생성 실패 (EnableInput 이후에도 null)"), FColor::Red);
		return;
	}

	InputComponent->BindKey(FireKey, IE_Pressed, this, &ADebugWeaponTester::HandleFirePressed);
	InputComponent->BindKey(ADSKey, IE_Pressed, this, &ADebugWeaponTester::HandleADSPressed);
	InputComponent->BindKey(ADSKey, IE_Released, this, &ADebugWeaponTester::HandleADSReleased);
	InputComponent->BindKey(ReloadKey, IE_Pressed, this, &ADebugWeaponTester::HandleReloadPressed);

	ShowDebugMessage(FString::Printf(TEXT("준비 완료: %s 장착, 좌클릭 발사/우클릭 ADS/R 재장전"), *WeaponClassToTest->GetName()), FColor::Green);
}

void ADebugWeaponTester::HandleFirePressed()
{
	if (!TestWeapon)
	{
		ShowDebugMessage(TEXT("Fire 입력 들어왔지만 TestWeapon이 null"), FColor::Red);
		return;
	}

	const bool bFired = TestWeapon->Fire();
	ShowDebugMessage(FString::Printf(TEXT("Fire() 호출 -> %s (탄약 %d, 재장전중 %s)"),
		bFired ? TEXT("성공") : TEXT("실패(쿨다운/탄약/재장전 확인)"),
		TestWeapon->GetCurrentAmmo(),
		TestWeapon->IsReloading() ? TEXT("Y") : TEXT("N")),
		bFired ? FColor::Cyan : FColor::Yellow);
}

void ADebugWeaponTester::HandleADSPressed()
{
	if (!TestWeapon)
	{
		ShowDebugMessage(TEXT("ADS 입력 들어왔지만 TestWeapon이 null"), FColor::Red);
		return;
	}

	TestWeapon->SetADS(true);
	ShowDebugMessage(FString::Printf(TEXT("ADS Pressed -> IsADS()=%s"), TestWeapon->IsADS() ? TEXT("true") : TEXT("false (bCanADS 확인)")), FColor::Cyan);
}

void ADebugWeaponTester::HandleADSReleased()
{
	if (!TestWeapon)
	{
		return;
	}

	TestWeapon->SetADS(false);
	ShowDebugMessage(TEXT("ADS Released"), FColor::Cyan);
}

void ADebugWeaponTester::HandleReloadPressed()
{
	if (TestWeapon)
	{
		TestWeapon->StartReload();
		ShowDebugMessage(TEXT("StartReload() 호출"), FColor::Cyan);
	}
}