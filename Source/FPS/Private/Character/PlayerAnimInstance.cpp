// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/PlayerAnimInstance.h"

#include "Character/ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"

void UPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
	if (ShooterCharacter)
	{
		MovementComponent = ShooterCharacter->GetCharacterMovement();
	}
}

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!ShooterCharacter)
	{
		ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
		if (ShooterCharacter)
		{
			MovementComponent = ShooterCharacter->GetCharacterMovement();
		}
	}

	if (!ShooterCharacter || !MovementComponent)
	{
		return;
	}

	const FVector Velocity = ShooterCharacter->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	bIsAccelerating = !MovementComponent->GetCurrentAcceleration().IsNearlyZero();
	bIsInAir = MovementComponent->IsFalling();
	Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, ShooterCharacter->GetActorRotation());
}
