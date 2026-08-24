// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/ShooterCharacter.h"

#include "AbilitySystemComponent.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "Camera/CameraComponent.h"
#include "Character/ShooterMovementComponent.h"
#include "Combat/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameMode/BaseRoundGameMode.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterCharacter, Log, All);

namespace
{
	AShooterPlayerState* ResolveShooterPlayerState(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		if (AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(Actor))
		{
			return PlayerState;
		}
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn->GetPlayerState<AShooterPlayerState>();
		}
		if (AController* Controller = Cast<AController>(Actor))
		{
			return Controller->GetPlayerState<AShooterPlayerState>();
		}

		return nullptr;
	}

	AShooterPlayerState* ResolveKillerPlayerState(const FRogueDeathContext& Context)
	{
		if (AShooterPlayerState* PlayerState = ResolveShooterPlayerState(Context.InstigatorActor.Get()))
		{
			return PlayerState;
		}

		AActor* Causer = Context.CauserActor.Get();
		if (AShooterPlayerState* PlayerState = ResolveShooterPlayerState(Causer))
		{
			return PlayerState;
		}
		if (!IsValid(Causer))
		{
			return nullptr;
		}

		if (AShooterPlayerState* PlayerState = ResolveShooterPlayerState(Causer->GetOwner()))
		{
			return PlayerState;
		}
		return ResolveShooterPlayerState(Causer->GetInstigator());
	}
}

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

void AShooterCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!IsValid(Movement))
	{
		return;
	}

	const bool bWasInAir = (PrevMovementMode == MOVE_Falling);
	const bool bIsInAir  = (Movement->MovementMode == MOVE_Falling);
	if (bWasInAir == bIsInAir)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		AbilitySystem->SetLooseGameplayTagCount(RogueTags::State_Movement_InAir, bIsInAir ? 1 : 0);
	}
}

void AShooterCharacter::CheckJumpInput(float DeltaTime)
{
	if (bPressedJump)
	{
		const ACharacter* DefaultCharacter = GetClass()->GetDefaultObject<ACharacter>();
		JumpMaxCount = DefaultCharacter->JumpMaxCount + URogueAugmentStatResolver::GetExtraJumpCount(this);
	}

	Super::CheckJumpInput(DeltaTime);
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

void AShooterCharacter::HandleDeath_Implementation(const FRogueDeathContext& Context)
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
		RoundGameMode->NotifyPlayerEliminatedBy(OwningController, ResolveKillerPlayerState(Context));
	}
}

void AShooterCharacter::PlayDeathReaction_Implementation(const FRogueDeathReaction& Reaction)
{
	if (bDeathStateApplied)
	{
		return;
	}
	bDeathStateApplied = true;

	// 관성 저장
	const FVector DeathVelocity = GetVelocity();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
	}

	// 캡슐 콜리전 비활성화
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 충돌 설정
	USkeletalMeshComponent* Mesh3P = GetMesh();
	Mesh3P->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Mesh3P->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh3P->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh3P->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Mesh3P->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	// 피직스 에셋이 없는 경우
	if (!Mesh3P->GetPhysicsAsset())
	{
		UE_LOG(
			LogShooterCharacter,
			Warning,
			TEXT("[ShooterCharacter] %s: 3인칭 메시에 PhysicsAsset 이 없어 랙돌을 시작할 수 없습니다 (메시=%s)"),
			*GetNameSafe(this),
			*GetNameSafe(Mesh3P->GetSkinnedAsset()));
	}

	// 데디 서버는 랙돌 X
	else if (GetNetMode() != NM_DedicatedServer)
	{
		Mesh3P->SetAllBodiesSimulatePhysics(true);
		Mesh3P->SetAllPhysicsLinearVelocity(DeathVelocity);

		ApplyDeathImpulse(*Mesh3P, Reaction);
	}

	// 본인의 1인칭 메시를 숨기고, 3인칭 메시를 보이게
	if (IsLocallyControlled())
	{
		Mesh1P->SetHiddenInGame(true, true);
		Mesh3P->SetOwnerNoSee(false);
	}
}

void AShooterCharacter::ApplyDeathImpulse(USkeletalMeshComponent& Mesh3P, const FRogueDeathReaction& Reaction) const
{
	// 임펄스 방향이 없거나 속도가 0이면 적용하지 않는다
	if (Reaction.Direction.IsNearlyZero() || DeathImpulseSpeed <= 0.f)
	{
		return;
	}

	// 임펄스를 적용할 본
	const FName FallbackBone = Reaction.bHeadshot ? HeadshotDeathImpulseBoneName : DeathImpulseBoneName;

	FName TargetBone = NAME_None;
	for (const FName Candidate : { Reaction.HitBoneName, FallbackBone })
	{
		if (!Candidate.IsNone() && Mesh3P.GetBodyInstance(Candidate))
		{
			TargetBone = Candidate;
			break;
		}
	}

	FVector ImpulseDirection = Reaction.Direction;
	ImpulseDirection.Z += DeathImpulseUpwardBias;
	if (!ImpulseDirection.Normalize())
	{
		return;
	}

	const float ImpulseSpeed = DeathImpulseSpeed
		* (Reaction.bHeadshot ? HeadshotDeathImpulseMultiplier : 1.f);

	// 임펄스 적용
	Mesh3P.AddImpulse(ImpulseDirection * ImpulseSpeed, TargetBone, true);
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
