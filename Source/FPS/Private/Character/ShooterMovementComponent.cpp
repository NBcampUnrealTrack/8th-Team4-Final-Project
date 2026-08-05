#include "Character/ShooterMovementComponent.h"

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

