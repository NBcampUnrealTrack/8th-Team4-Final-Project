#include "Animation/ShooterAnimInstance.h"

#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

void UShooterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CacheReferences();
	if (ShooterCharacter)
	{
		StartingAimRotation = FRotator(0.f, ShooterCharacter->GetBaseAimRotation().Yaw, 0.f);
	}
}

void UShooterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!ShooterCharacter)
	{
		CacheReferences();
	}
}

void UShooterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (!ShooterCharacter || !CharacterMovementComponent)
	{
		return;
	}

	UpdateMovementValues();
	UpdateAimValues(DeltaSeconds);
	UpdateWeaponValues();
}

void UShooterAnimInstance::CacheReferences()
{
	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
	if (!ShooterCharacter)
	{
		CharacterMovementComponent = nullptr;
		Combat = nullptr;
		return;
	}

	CharacterMovementComponent = ShooterCharacter->GetCharacterMovement();
	Combat = ShooterCharacter->FindComponentByClass<UCombatComponent>();
}

void UShooterAnimInstance::UpdateMovementValues()
{
	const FVector HorizontalVelocity(ShooterCharacter->GetVelocity().X, ShooterCharacter->GetVelocity().Y, 0.f);
	GroundSpeed = HorizontalVelocity.Size();
	bIsMoving = GroundSpeed > 3.f;
	bIsFalling = CharacterMovementComponent->IsFalling();
	bIsCrouching = ShooterCharacter->bIsCrouched;
	bIsAccelerating = CharacterMovementComponent->GetCurrentAcceleration().SizeSquared() > 0.f;

	if (bIsMoving)
	{
		const FRotator AimYawRotation(0.f, ShooterCharacter->GetBaseAimRotation().Yaw, 0.f);
		const FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(HorizontalVelocity);
		MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimYawRotation).Yaw;
	}
	else
	{
		MovementOffsetYaw = 0.f;
	}
}

void UShooterAnimInstance::UpdateAimValues(float DeltaSeconds)
{
	AimRotation = ShooterCharacter->CalculateFixedAimRotation();
	AO_Pitch = AimRotation.Pitch;

	if (!bIsMoving && !bIsFalling)
	{
		const FRotator CurrentAimRotation(0.f, ShooterCharacter->GetBaseAimRotation().Yaw, 0.f);
		const FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;

		if (TurningStatus == ETurningInPlace::NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}

		TurnInPlace(DeltaSeconds);
		return;
	}

	StartingAimRotation = FRotator(0.f, ShooterCharacter->GetBaseAimRotation().Yaw, 0.f);
	AO_Yaw = 0.f;
	InterpAO_Yaw = 0.f;
	TurningStatus = ETurningInPlace::NotTurning;
}

void UShooterAnimInstance::UpdateWeaponValues()
{
	bAiming = IsValid(Combat) && Combat->bAiming;
	CurrentWeapon = IsValid(Combat) ? Combat->GetCurrentWeapon() : nullptr;
	bHasCurrentWeapon = IsValid(CurrentWeapon);
	CurrentWeaponType = bHasCurrentWeapon ? CurrentWeapon->WeaponType : ShooterTags::TAG_WeaponType_None;
}

void UShooterAnimInstance::TurnInPlace(float DeltaSeconds)
{
	if (AO_Yaw > 90.f)
	{
		TurningStatus = ETurningInPlace::Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningStatus = ETurningInPlace::Left;
	}

	if (TurningStatus != ETurningInPlace::NotTurning)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaSeconds, 4.f);
		AO_Yaw = InterpAO_Yaw;

		if (FMath::Abs(AO_Yaw) < 5.f)
		{
			TurningStatus = ETurningInPlace::NotTurning;
			StartingAimRotation = FRotator(0.f, ShooterCharacter->GetBaseAimRotation().Yaw, 0.f);
		}
	}
}
