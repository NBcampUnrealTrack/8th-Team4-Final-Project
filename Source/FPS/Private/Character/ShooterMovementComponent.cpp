#include "Character/ShooterMovementComponent.h"

#include "Augment/RogueAugmentStatResolver.h"
#include "Combat/CombatComponent.h"
#include "Weapon/WeaponBase.h"

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
	if (IsCrouching())
	{
		return;
	}

	bWantsToSprint = true;
	MaxWalkSpeed = SprintSpeed;
}

void UShooterMovementComponent::StopSprint()
{
	bWantsToSprint = false;
	MaxWalkSpeed = WalkSpeed;
}


float UShooterMovementComponent::GetMaxSpeed() const
{
	const float BaseMaxSpeed = Super::GetMaxSpeed();
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return BaseMaxSpeed;
	}

	const float ResolvedMaxSpeed = bWantsToSprint
		? URogueAugmentStatResolver::GetFinalSprintSpeed(OwnerActor, BaseMaxSpeed)
		: URogueAugmentStatResolver::GetFinalMoveSpeed(OwnerActor, BaseMaxSpeed);

	// 무기별 이동속도 배율(예: 저격소총은 느리게, 칼/권총은 빠르게). 장착 무기가 없거나 배율이
	// 기본값(1)이면 영향이 없다.
	if (const UCombatComponent* Combat = OwnerActor->FindComponentByClass<UCombatComponent>())
	{
		if (const AWeaponBase* Weapon = Combat->GetEquippedWeapon())
		{
			return ResolvedMaxSpeed * Weapon->GetMoveSpeedMultiplier();
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
