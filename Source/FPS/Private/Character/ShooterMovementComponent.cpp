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
