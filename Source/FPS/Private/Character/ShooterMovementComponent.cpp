#include "Character/ShooterMovementComponent.h"

#include "Augment/RogueAugmentStatResolver.h"
#include "Combat/CombatComponent.h"
#include "Weapon/WeaponBase.h"
#include "GameFramework/Character.h"

void FSavedMove_Shooter::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = 0;
}

uint8 FSavedMove_Shooter::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}
	return Result;
}

bool FSavedMove_Shooter::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	if (bSavedWantsToSprint != static_cast<const FSavedMove_Shooter*>(NewMove.Get())->bSavedWantsToSprint)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Shooter::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UShooterMovementComponent* Movement = Cast<UShooterMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Movement->bWantsToSprint;
	}
}

void FSavedMove_Shooter::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UShooterMovementComponent* Movement = Cast<UShooterMovementComponent>(C->GetCharacterMovement()))
	{
		Movement->bWantsToSprint = bSavedWantsToSprint;
	}
}

FNetworkPredictionData_Client_Shooter::FNetworkPredictionData_Client_Shooter(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_Shooter::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Shooter());
}

UShooterMovementComponent::UShooterMovementComponent()
{
	MaxWalkSpeed = WalkSpeed;
	MaxWalkSpeedCrouched = CrouchSpeed;
	SetCrouchedHalfHeight(CrouchHalfHeight);
	NavAgentProps.bCanCrouch = true;
}

void UShooterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxWalkSpeed = WalkSpeed;
	MaxWalkSpeedCrouched = CrouchSpeed;
	SetCrouchedHalfHeight(CrouchHalfHeight);
	NavAgentProps.bCanCrouch = true;
}

void UShooterMovementComponent::StartSprint()
{
	if (IsCrouching() || bSprintBlocked)
	{
		return;
	}

	bWantsToSprint = true;
}

void UShooterMovementComponent::StopSprint()
{
	bWantsToSprint = false;
}

void UShooterMovementComponent::SetSprintBlocked(bool bBlocked)
{
	bSprintBlocked = bBlocked;
	if (bSprintBlocked)
	{
		bWantsToSprint = false;
	}
}

FNetworkPredictionData_Client* UShooterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);

	if (!ClientPredictionData)
	{
		UShooterMovementComponent* MutableThis = const_cast<UShooterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Shooter(*this);
	}
	return ClientPredictionData;
}

void UShooterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0 && !bSprintBlocked;
}

bool UShooterMovementComponent::CanCrouchInCurrentState() const
{
	// Enter crouch on the ground; keep an existing crouch when walking off a ledge.
	return Super::CanCrouchInCurrentState() && (IsMovingOnGround() || IsCrouching());
}

void UShooterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Discard disallowed requests, including replayed moves, instead of buffering them until landing.
	if (!CanCrouchInCurrentState())
	{
		bWantsToCrouch = false;
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

float UShooterMovementComponent::GetMaxSpeed() const
{
	const bool bSprinting = bWantsToSprint && !IsCrouching();
	const float BaseMaxSpeed = bSprinting ? SprintSpeed : Super::GetMaxSpeed();
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return BaseMaxSpeed;
	}

	const float ResolvedMaxSpeed = bSprinting
		? URogueAugmentStatResolver::GetFinalSprintSpeed(OwnerActor, BaseMaxSpeed)
		: URogueAugmentStatResolver::GetFinalMoveSpeed(OwnerActor, BaseMaxSpeed);

	// 무기별 이동속도 배율(예: 저격소총은 느리게, 칼/권총은 빠르게). 장착 무기가 없거나 배율이
	// 기본값(1)이면 영향이 없다.
	if (const UCombatComponent* Combat = OwnerActor->FindComponentByClass<UCombatComponent>())
	{
		if (const AWeaponBase* Weapon = Combat->GetEquippedWeapon())
		{
			float SpeedMultiplier = Weapon->GetMoveSpeedMultiplier();
			// ADS(조준) 중에는 무기별 ADS 이속 배율을 추가로 곱한다 - 라이플/스나이퍼처럼 ADS가
			// 가능한 무기만 영향받는다(CanADS()가 false인 무기는 애초에 IsADS()가 true가 될 수 없다).
			if (Combat->IsAiming())
			{
				SpeedMultiplier *= Weapon->GetADSMoveSpeedMultiplier();
			}
			return ResolvedMaxSpeed * SpeedMultiplier;
		}
	}

	return ResolvedMaxSpeed;
}

bool UShooterMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return Super::DoJump(bReplayingMoves, DeltaTime);
	}

	TGuardValue<float> JumpVelocityGuard(
		JumpZVelocity,
		URogueAugmentStatResolver::GetFinalJumpVelocity(OwnerActor, JumpZVelocity));

	return Super::DoJump(bReplayingMoves, DeltaTime);
}
