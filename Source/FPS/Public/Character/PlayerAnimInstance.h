// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerAnimInstance.generated.h"

class AShooterCharacter;
class UCharacterMovementComponent;

UCLASS()
class FPS_API UPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation")
	TObjectPtr<AShooterCharacter> ShooterCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation")
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation")
	float Direction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation")
	bool bIsAccelerating = false;
};
