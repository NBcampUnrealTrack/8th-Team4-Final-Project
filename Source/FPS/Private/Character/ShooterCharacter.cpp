// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/ShooterCharacter.h"

#include "Camera/CameraComponent.h"
#include "Character/ShooterMovementComponent.h"
#include "Combat/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFlow/Round/BaseRoundGameMode.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Player/ShooterPlayerState.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterCharacter, Log, All);

AShooterCharacter::AShooterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShooterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 0.f;
	SpringArm->bUsePawnControlRotation = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(SpringArm);
	FirstPersonCamera->bUsePawnControlRotation = false;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(FirstPersonCamera);
	Mesh1P->bOnlyOwnerSee = true;
	Mesh1P->bOwnerNoSee = false;
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->bReceivesDecals = false;

	GetMesh()->bOnlyOwnerSee = false;
	GetMesh()->bOwnerNoSee = true;
	GetMesh()->bReceivesDecals = false;

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
	Combat->SetIsReplicated(true);
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		InitializeDefaultWeaponLoadout();
	}
}

bool AShooterCharacter::InitializeDefaultWeaponLoadout()
{
	if (!HasAuthority() || !IsValid(Combat) || !DefaultPrimaryWeaponClass)
	{
		return false;
	}

	if (IsValid(Combat->GetWeaponInSlot(EWeaponSlot::Primary)))
	{
		return true;
	}

	if (!Combat->InitializeWeaponLoadout(DefaultPrimaryWeaponClass, DefaultSecondaryWeaponClass, DefaultMeleeWeaponClass))
	{
		UE_LOG(
			LogShooterCharacter,
			Error,
			TEXT("[ShooterCharacter] %s: 기본 무기 로드아웃 초기화 실패. Primary=%s Secondary=%s Melee=%s"),
			*GetNameSafe(this),
			*GetNameSafe(DefaultPrimaryWeaponClass.Get()),
			*GetNameSafe(DefaultSecondaryWeaponClass.Get()),
			*GetNameSafe(DefaultMeleeWeaponClass.Get()));
		return false;
	}

	UE_LOG(
		LogShooterCharacter,
		Log,
		TEXT("[ShooterCharacter] %s: 기본 무기 로드아웃 준비 완료. Primary=%s Secondary=%s Melee=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Combat->GetWeaponInSlot(EWeaponSlot::Primary)),
		*GetNameSafe(Combat->GetWeaponInSlot(EWeaponSlot::Secondary)),
		*GetNameSafe(Combat->GetWeaponInSlot(EWeaponSlot::Melee)));

	return true;
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

UAbilitySystemComponent* AShooterCharacter::GetAbilitySystemComponent() const
{
	const AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>();

	return ShooterPlayerState ? ShooterPlayerState->GetAbilitySystemComponent() : nullptr;
}

USkeletalMeshComponent* AShooterCharacter::GetMesh1P_Implementation() const
{
	return Mesh1P;
}

USkeletalMeshComponent* AShooterCharacter::GetMesh3P_Implementation() const
{
	return GetMesh();
}

UCameraComponent* AShooterCharacter::GetViewCamera_Implementation() const
{
	return FirstPersonCamera;
}

FRotator AShooterCharacter::GetFixedAimRotation_Implementation() const
{
	return CalculateFixedAimRotation();
}

AWeaponBase* AShooterCharacter::GetCurrentWeapon_Implementation() const
{
	return IsValid(Combat) ? Combat->GetCurrentWeapon() : nullptr;
}

void AShooterCharacter::HandleDeath_Implementation(const FRogueDeathContext&)
{
	if (!HasAuthority())
	{
		return;
	}

	AController* OwningController = GetController();
	UWorld* World = GetWorld();
	ABaseRoundGameMode* RoundGameMode = IsValid(World) ? World->GetAuthGameMode<ABaseRoundGameMode>() : nullptr;
	if (IsValid(OwningController) && IsValid(RoundGameMode))
	{
		RoundGameMode->NotifyPlayerEliminated(OwningController);
	}
}

bool AShooterCharacter::HasCurrentWeapon() const
{
	return IsValid(Combat) && IsValid(Combat->GetCurrentWeapon());
}

FRotator AShooterCharacter::CalculateFixedAimRotation() const
{
	FRotator AimRotation = GetBaseAimRotation();
	if (AimRotation.Pitch > 90.f && !IsLocallyControlled())
	{
		const FVector2D InRange(270.f, 360.f);
		const FVector2D OutRange(-90.f, 0.f);
		AimRotation.Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AimRotation.Pitch);
	}

	return AimRotation;
}

void AShooterCharacter::HandleMoveInput(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (!Controller || MovementVector.IsNearlyZero())
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementVector.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementVector.X);
}

void AShooterCharacter::HandleLookInput(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void AShooterCharacter::HandleJumpPressed()
{
	Jump();
}

void AShooterCharacter::HandleJumpReleased()
{
	StopJumping();
}

void AShooterCharacter::HandleSprintPressed()
{
	if (UShooterMovementComponent* ShooterMovement = Cast<UShooterMovementComponent>(GetCharacterMovement()))
	{
		ShooterMovement->StartSprint();
	}
}

void AShooterCharacter::HandleSprintReleased()
{
	if (UShooterMovementComponent* ShooterMovement = Cast<UShooterMovementComponent>(GetCharacterMovement()))
	{
		ShooterMovement->StopSprint();
	}
}

// 무기 슬롯 전환은 UGA_SwitchWeapon(GAS 어빌리티)이 담당한다 - Fire/Reload와 동일하게
// 입력 태그 -> 어빌리티 경로를 타며, Character는 더 이상 스왑을 직접 호출하지 않는다.
