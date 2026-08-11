// Fill out your copyright notice in the Description page of Project Settings.

#include "Debug/DebugCombatTester.h"

#include "Camera/CameraComponent.h"
#include "Combat/CombatComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Weapon/WeaponBase.h"

namespace
{
	void ShowCombatTesterDebugMessage(const FString& Message, const FColor& Color)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugCombatTester] %s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, Color, FString::Printf(TEXT("[DebugCombatTester] %s"), *Message));
		}
	}
}

ADebugCombatTester::ADebugCombatTester()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// ShooterCharacter::Mesh1P와 동일한 가시성 규칙 (오너만 봄).
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(SceneRoot);
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetOwnerNoSee(false);

	// ShooterCharacter::GetMesh()(3인칭 바디)와 동일한 가시성 규칙 (오너에게는 숨김).
	Mesh3P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh3P"));
	Mesh3P->SetupAttachment(SceneRoot);
	Mesh3P->SetOnlyOwnerSee(false);
	Mesh3P->SetOwnerNoSee(true);

	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
}

USkeletalMeshComponent* ADebugCombatTester::GetMesh1P_Implementation() const
{
	return Mesh1P;
}

USkeletalMeshComponent* ADebugCombatTester::GetMesh3P_Implementation() const
{
	return Mesh3P;
}

UCameraComponent* ADebugCombatTester::GetViewCamera_Implementation() const
{
	// 이 테스트 액터는 카메라를 검증하지 않는다 (부착 위치만 확인).
	return nullptr;
}

FRotator ADebugCombatTester::GetFixedAimRotation_Implementation() const
{
	return FRotator::ZeroRotator;
}

void ADebugCombatTester::BeginPlay()
{
	Super::BeginPlay();

	if (!WeaponClassToTest)
	{
		ShowCombatTesterDebugMessage(TEXT("WeaponClassToTest가 비어있음 - 배치한 액터의 Details에서 지정할 것"), FColor::Red);
		return;
	}

	if (!Mesh1P->GetSkeletalMeshAsset() || !Mesh3P->GetSkeletalMeshAsset())
	{
		ShowCombatTesterDebugMessage(TEXT("Mesh1P/Mesh3P에 스켈레탈 메쉬가 없음 - SK_Mannequin_Arms / SK_Mannequin을 지정할 것"), FColor::Red);
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	if (PlayerPawn)
	{
		// bOnlyOwnerSee/bOwnerNoSee는 "이 액터가 로컬 플레이어가 조종 중인 폰의 소유 체인에 있는지"로
		// 가시성을 판정한다. 이 테스터는 레벨에 그냥 놓인 액터라 Owner가 없으면 Mesh1P가 아무에게도
		// 안 보인다 - 로컬 Pawn을 Owner로 잡아야 화면에서 확인 가능하다.
		SetOwner(PlayerPawn);

		// Pawn 루트에 오프셋 없이 붙이면 팔 메쉬 원점이 카메라 한복판에 박혀서 화면을 뒤덮는다.
		// 카메라가 있으면 카메라에 붙이고 뷰모델처럼 앞쪽으로 살짝 띄운다(정밀 튜닝용이 아니라
		// "손에 총이 붙어 보이는지"만 확인하기 위한 대략적인 오프셋).
		if (UCameraComponent* PlayerCamera = PlayerPawn->FindComponentByClass<UCameraComponent>())
		{
			AttachToComponent(PlayerCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			SetActorRelativeLocation(FVector(30.f, 10.f, -20.f));
			SetActorRelativeRotation(FRotator::ZeroRotator);
		}
		else
		{
			AttachToActor(PlayerPawn, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			ShowCombatTesterDebugMessage(TEXT("PlayerPawn에 CameraComponent가 없음 - Pawn 루트에 그대로 붙임(위치가 어색할 수 있음)"), FColor::Yellow);
		}
	}
	else
	{
		ShowCombatTesterDebugMessage(TEXT("PlayerPawn을 못 찾음 - Owner 미설정으로 Mesh1P(OnlyOwnerSee)가 안 보일 수 있음"), FColor::Yellow);
	}

	EquipTestWeapon();

	if (!PC)
	{
		ShowCombatTesterDebugMessage(TEXT("PlayerController를 못 찾음 - RequipKey 재검증은 불가 (PIE 확인)"), FColor::Yellow);
		return;
	}

	EnableInput(PC);
	if (InputComponent)
	{
		InputComponent->BindKey(RequipKey, IE_Pressed, this, &ADebugCombatTester::HandleRequipPressed);
	}
}

void ADebugCombatTester::EquipTestWeapon()
{
	if (!CombatComponent)
	{
		return;
	}

	AWeaponBase* Weapon = CombatComponent->EquipWeapon(WeaponClassToTest);
	if (!Weapon)
	{
		ShowCombatTesterDebugMessage(TEXT("EquipWeapon 실패"), FColor::Red);
		return;
	}

	ShowCombatTesterDebugMessage(FString::Printf(TEXT("EquipWeapon 성공: %s (GripSocket1P=%s, GripSocket3P=%s)"),
		*WeaponClassToTest->GetName(),
		*Weapon->GetGripSocketName1P().ToString(),
		*Weapon->GetGripSocketName3P().ToString()),
		FColor::Green);
}

void ADebugCombatTester::HandleRequipPressed()
{
	ShowCombatTesterDebugMessage(TEXT("RequipKey 입력 -> Unequip 후 재장착"), FColor::Cyan);
	EquipTestWeapon();
}