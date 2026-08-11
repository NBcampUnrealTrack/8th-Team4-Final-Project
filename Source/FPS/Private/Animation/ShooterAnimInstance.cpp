#include "Animation/ShooterAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Data/WeaponData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAnimation, Log, All);

void UShooterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CacheReferences();
	LastAnimationWeapon.Reset();
	bWasReloading = false;
	UpdateWeaponValues();
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

	UpdateWeaponValues();
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
	bFirstPersonAnimation = GetSkelMeshComponent() == ShooterCharacter->GetMesh1P_Implementation();
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
	bIsCycling = IsValid(Combat) && Combat->IsCycling();
	bWeaponReady = IsValid(Combat) && Combat->IsWeaponReady();
	bIsReloading = IsValid(CurrentWeapon) && CurrentWeapon->IsReloading();

	const FGameplayTag WeaponTag = IsValid(CurrentWeapon) ? CurrentWeapon->GetWeaponTag() : FGameplayTag();
	CurrentWeaponType = WeaponTag.IsValid() ? WeaponTag : ShooterTags::TAG_WeaponType_None;

	UpdateWeaponAnimationData();

	if (CurrentWeapon.Get() != LastAnimationWeapon.Get())
	{
		LastAnimationWeapon = CurrentWeapon.Get();
		bWasReloading = false;
		PlayEquipMontageForCurrentWeapon();
	}

	if (bIsReloading && !bWasReloading)
	{
		PlayReloadMontageForCurrentWeapon();
	}

	bWasReloading = bIsReloading;
}

void UShooterAnimInstance::UpdateWeaponAnimationData()
{
	CurrentIdleAnim = nullptr;
	CurrentAimIdleAnim = nullptr;

	const UWeaponData* WeaponData = IsValid(Combat) ? Combat->GetWeaponData() : nullptr;
	if (!IsValid(WeaponData) || !CurrentWeaponType.IsValid())
	{
		return;
	}

	if (const FPlayerAnims* PlayerAnims = WeaponData->FindPlayerAnims(CurrentWeaponType, bFirstPersonAnimation))
	{
		CurrentIdleAnim = PlayerAnims->IdleAnim;
		CurrentAimIdleAnim = PlayerAnims->AimIdleAnim;
	}
}

void UShooterAnimInstance::PlayEquipMontageForCurrentWeapon()
{
	if (!IsValid(CurrentWeapon) || !IsValid(Combat))
	{
		return;
	}

	const UWeaponData* WeaponData = Combat->GetWeaponData();
	if (!IsValid(WeaponData))
	{
		return;
	}

	const FWeaponMontageData* MontageData = WeaponData->FindWeaponMontages(CurrentWeaponType, bFirstPersonAnimation);
	if (!MontageData || !IsValid(MontageData->EquipMontage))
	{
		return;
	}

	const float PlayedDuration = Montage_Play(MontageData->EquipMontage);
	if (PlayedDuration <= 0.f)
	{
		UE_LOG(
			LogShooterAnimation,
			Warning,
			TEXT("[ShooterAnimInstance] Equip montage could not be played. Owner=%s WeaponTag=%s View=%s Montage=%s"),
			*GetNameSafe(ShooterCharacter.Get()),
			*CurrentWeaponType.ToString(),
			bFirstPersonAnimation ? TEXT("1P") : TEXT("3P"),
			*GetNameSafe(MontageData->EquipMontage));
	}
}

void UShooterAnimInstance::PlayReloadMontageForCurrentWeapon()
{
	if (!IsValid(CurrentWeapon) || !IsValid(Combat))
	{
		return;
	}

	const UWeaponData* WeaponData = Combat->GetWeaponData();
	if (!IsValid(WeaponData))
	{
		return;
	}

	const FWeaponMontageData* MontageData = WeaponData->FindWeaponMontages(CurrentWeaponType, bFirstPersonAnimation);
	if (!MontageData || !IsValid(MontageData->ReloadMontage))
	{
		return;
	}

	if (Montage_IsPlaying(MontageData->ReloadMontage))
	{
		return;
	}

	const float PlayedDuration = Montage_Play(MontageData->ReloadMontage);
	if (PlayedDuration <= 0.f)
	{
		UE_LOG(
			LogShooterAnimation,
			Warning,
			TEXT("[ShooterAnimInstance] Reload montage could not be played. Owner=%s WeaponTag=%s View=%s Montage=%s"),
			*GetNameSafe(ShooterCharacter.Get()),
			*CurrentWeaponType.ToString(),
			bFirstPersonAnimation ? TEXT("1P") : TEXT("3P"),
			*GetNameSafe(MontageData->ReloadMontage));
	}
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
