#include "Character/ShooterMovementComponent.h"

#include "Augment/RogueAugmentStatResolver.h"

UShooterMovementComponent::UShooterMovementComponent()
{
	MaxWalkSpeed = WalkSpeed;
}

void UShooterMovementComponent::StartSprint()
{
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

	return bWantsToSprint
		? URogueAugmentStatResolver::GetFinalSprintSpeed(OwnerActor, BaseMaxSpeed)
		: URogueAugmentStatResolver::GetFinalMoveSpeed(OwnerActor, BaseMaxSpeed);
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
