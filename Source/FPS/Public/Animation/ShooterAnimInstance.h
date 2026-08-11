#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Character/ShooterCharacterTypes.h"
#include "GameplayTagContainer.h"
#include "ShooterAnimInstance.generated.h"

class AShooterCharacter;
class AWeaponBase;
class UAnimMontage;
class UAnimSequence;
class UCharacterMovementComponent;
class UCombatComponent;

UCLASS()
class FPS_API UShooterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|References")
	TObjectPtr<AShooterCharacter> ShooterCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|References")
	TObjectPtr<UCharacterMovementComponent> CharacterMovementComponent;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|References")
	TObjectPtr<UCombatComponent> Combat;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	TObjectPtr<AWeaponBase> CurrentWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	bool bHasCurrentWeapon = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	FGameplayTag CurrentWeaponType;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	TObjectPtr<UAnimSequence> CurrentIdleAnim;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	TObjectPtr<UAnimSequence> CurrentAimIdleAnim;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	bool bIsCycling = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	bool bWeaponReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	bool bFirstPersonAnimation = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Combat")
	bool bAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	bool bIsCrouching = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Aiming")
	FRotator AimRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Aiming")
	float AO_Yaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Aiming")
	float AO_Pitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	float MovementOffsetYaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|TurnInPlace")
	ETurningInPlace TurningStatus = ETurningInPlace::NotTurning;

private:
	void CacheReferences();
	void UpdateMovementValues();
	void UpdateAimValues(float DeltaSeconds);
	void UpdateWeaponValues();
	void UpdateWeaponAnimationData();
	void PlayEquipMontageForCurrentWeapon();
	void TurnInPlace(float DeltaSeconds);

	FRotator StartingAimRotation = FRotator::ZeroRotator;
	float InterpAO_Yaw = 0.f;
	TWeakObjectPtr<AWeaponBase> LastAnimationWeapon;
};
