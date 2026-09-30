#include "Animation/ShooterAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/WeaponData.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Player/DominationPlayerState.h"
#include "Tags/GameplayTags.h"
#include "Training/CQB/CQBTrainingAIComponent.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAnimation, Log, All);

void UShooterAnimInstance::NativeInitializeAnimation()
{
	RestoreSupportAnchor();
	UnregisterSupportAnchor();
	PreviousWeaponMesh.Reset();
	bThirdPersonSupportAnchor = false;
	Super::NativeInitializeAnimation();

	CacheReferences();
	if (!bFirstPersonAnimation && GetSkelMeshComponent())
	{
		SupportAnchorBody = GetSkelMeshComponent();
		SupportAnchorFinalizedHandle = GetSkelMeshComponent()->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &UShooterAnimInstance::UpdateSupportAnchor));
	}
	LastAnimationWeapon.Reset();
	bWasReloading = false;
	bWasUtilityHeld = false;
	UpdateWeaponValues();
	if (ShooterCharacter)
	{
		StartingAimRotation = FRotator(0.f, ShooterCharacter->GetBaseAimRotation().Yaw, 0.f);
	}
}

void UShooterAnimInstance::RestoreSupportAnchor()
{
	if (bSupportAnchorActive)
	{
		if (USkeletalMeshComponent* WeaponMesh = SupportAnchorMesh.Get())
		{
			// Do not undo a reattachment performed by equip/drop while the action ended.
			if (WeaponMesh->GetAttachParent() == GetSkelMeshComponent()
				&& WeaponMesh->GetAttachSocketName() == OriginalWeaponSocket)
			{
				WeaponMesh->SetRelativeTransform(OriginalWeaponRelative);
			}
		}
	}
	bSupportAnchorActive = false;
	SupportAnchorMesh.Reset();
}

void UShooterAnimInstance::NativeUninitializeAnimation()
{
	RestoreSupportAnchor();
	UnregisterSupportAnchor();
	PreviousWeaponMesh.Reset();
	Super::NativeUninitializeAnimation();
}

void UShooterAnimInstance::UnregisterSupportAnchor()
{
	if (USkeletalMeshComponent* Body = SupportAnchorBody.Get())
	{
		Body->UnregisterOnBoneTransformsFinalizedDelegate(SupportAnchorFinalizedHandle);
	}
	SupportAnchorFinalizedHandle.Reset();
	SupportAnchorBody.Reset();
}

void UShooterAnimInstance::UpdateSupportAnchor()
{
	// Finalized bones (after buffer flip), not PostEvaluate's previous-frame sockets.
	USkeletalMeshComponent* Body = GetSkelMeshComponent();
	USkeletalMeshComponent* WeaponMesh = IsValid(CurrentWeapon) ? CurrentWeapon->GetMesh3P() : nullptr;
	const bool bEligible = !bFirstPersonAnimation && Body && WeaponMesh
		&& WeaponMesh->GetAttachParent() == Body && Body->DoesSocketExist(TEXT("hand_l"));
	if (!bEligible || !bThirdPersonSupportAnchor || bIsReloading
		|| (bSupportAnchorActive && SupportAnchorMesh.Get() != WeaponMesh))
	{
		RestoreSupportAnchor();
	}
	if (!bEligible)
	{
		PreviousWeaponMesh.Reset();
		return;
	}
	const FTransform SupportHand = Body->GetSocketTransform(TEXT("hand_l"));
	// Socket-attached children are updated after this callback. Reconstruct their
	// current-frame world transform instead of sampling the stale child transform.
	const FTransform AttachedWeaponWorld = WeaponMesh->GetRelativeTransform()
		* Body->GetSocketTransform(WeaponMesh->GetAttachSocketName());
	if (bThirdPersonSupportAnchor && !bIsReloading)
	{
		if (!bSupportAnchorActive)
		{
			OriginalWeaponRelative = WeaponMesh->GetRelativeTransform();
			OriginalWeaponSocket = WeaponMesh->GetAttachSocketName();
			SupportAnchorLocal = PreviousWeaponMesh.Get() == WeaponMesh
				? PreviousSupportRelative : AttachedWeaponWorld.GetRelativeTransform(SupportHand);
			SupportAnchorMesh = WeaponMesh;
			bSupportAnchorActive = true;
		}
		WeaponMesh->SetWorldTransform(SupportAnchorLocal * SupportHand);
	}
	else
	{
		PreviousSupportRelative = AttachedWeaponWorld.GetRelativeTransform(SupportHand);
		PreviousWeaponMesh = WeaponMesh;
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

float UShooterAnimInstance::GetIdlePostureSourceWeight() const
{
	return 1.f - FMath::Clamp(GetSlotMontageGlobalWeight(TEXT("DefaultSlot")), 0.f, 1.f);
}

float UShooterAnimInstance::GetEquipAwarePostureSourceWeight() const
{
	const float BaseWeight = GetIdlePostureSourceWeight();
	if (bFirstPersonAnimation || !IsValid(Combat))
	{
		return BaseWeight;
	}
	const UWeaponData* Data = Combat->GetWeaponData();
	const UAnimMontage* Active = nullptr;
	// IsActive excludes natural blend-out, but its pose is still on screen.
	// Newest DefaultSlot action wins, including its blend-out. This keeps a
	// completed Draw steady without overriding a newer fire/reload action.
	for (int32 Index = MontageInstances.Num() - 1; Index >= 0; --Index)
	{
		const FAnimMontageInstance* Instance = MontageInstances[Index];
		if (Instance && Instance->IsValid() && Instance->Montage->SlotAnimTracks.ContainsByPredicate(
			[](const FSlotAnimationTrack& Slot) { return Slot.SlotName == TEXT("DefaultSlot"); }))
		{
			Active = Instance->Montage;
			break;
		}
	}
	if (!IsValid(Data) || !Active || !CurrentWeaponType.IsValid())
	{
		return BaseWeight;
	}
	// Match the data-selected equip asset, including remote class overrides.
	for (const auto& Class : Data->ClassAnimationOverrides)
	{
		if (const auto* Equip = Class.Value.ThirdPersonEquipMontages.Find(CurrentWeaponType))
		{
			if (Equip->Get() == Active) return 1.f;
		}
	}
	const auto* Common = Data->FindWeaponMontages(CurrentWeaponType, false);
	return Common && Common->EquipMontage == Active ? 1.f : BaseWeight;
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
		bCQBTrainingAI = false;
		return;
	}

	CharacterMovementComponent = ShooterCharacter->GetCharacterMovement();
	Combat = ShooterCharacter->FindComponentByClass<UCombatComponent>();
	bFirstPersonAnimation = GetSkelMeshComponent() == ShooterCharacter->GetMesh1P_Implementation();
	bCQBTrainingAI = ShooterCharacter->FindComponentByClass<UCQBTrainingAIComponent>() != nullptr;
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

	if (bCQBTrainingAI)
	{
		StartingAimRotation = FRotator(0.f, ShooterCharacter->GetActorRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		InterpAO_Yaw = 0.f;
		TurningStatus = ETurningInPlace::NotTurning;
		return;
	}

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
	CurrentWeaponType = WeaponTag.IsValid() ? WeaponTag : RogueTags::Weapon_Type_None;

	UpdateWeaponAnimationData();

	const bool bUtilityHeld = IsValid(Combat) && Combat->IsPresentationSuppressed();
	const bool bReleasedUtility = !bUtilityHeld && bWasUtilityHeld;

	if (CurrentWeapon.Get() != LastAnimationWeapon.Get() || bReleasedUtility)
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
	bWasUtilityHeld = bUtilityHeld;
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

	if (const FPlayerAnims* PlayerAnims = WeaponData->FindPlayerAnims(
		CurrentWeaponType,
		bFirstPersonAnimation,
		GetCurrentCombatClass()))
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

	EDominationCombatClass DrawClass = GetCurrentCombatClass();
	// Preview/training pawns and newly replicated pawns can precede PlayerState.
	if (!bFirstPersonAnimation && DrawClass == EDominationCombatClass::Unselected)
	{
		for (const auto& Definition : GetDefault<UDominationClassSettings>()->CombatClasses)
		{
			if (Definition.PawnClass.Get() && ShooterCharacter->IsA(Definition.PawnClass.Get()))
			{
				DrawClass = Definition.CombatClass;
				break;
			}
		}
	}
	UAnimMontage* EquipMontage = WeaponData->FindEquipMontage(CurrentWeaponType, bFirstPersonAnimation, DrawClass);
	if (!IsValid(EquipMontage))
	{
		return;
	}

	const float PlayedDuration = Montage_Play(EquipMontage, UWeaponData::GetSwitchAnimationRate(CurrentWeaponType));
	if (PlayedDuration <= 0.f)
	{
		UE_LOG(
			LogShooterAnimation,
			Warning,
			TEXT("[ShooterAnimInstance] Equip montage could not be played. Owner=%s WeaponTag=%s View=%s Montage=%s"),
			*GetNameSafe(ShooterCharacter.Get()),
			*CurrentWeaponType.ToString(),
			bFirstPersonAnimation ? TEXT("1P") : TEXT("3P"),
			*GetNameSafe(EquipMontage));
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

	// Fire 차단 해제 시점은 몽타주 재생과 무관하게 AWeaponBase::EffectiveReloadDuration 타이머로
	// 결정된다(CanFireNow -> bIsReloading). 몽타주 원본 길이가 그 값과 다르면(무기별 ReloadDuration
	// 세팅이 실제 애니메이션 길이와 어긋난 경우) 애니메이션이 끝나기 전에 Fire가 풀리는 문제가 생기므로,
	// PlayRate를 조정해서 몽타주가 항상 EffectiveReloadDuration과 같은 시간에 끝나도록 맞춘다.
	const float MontageLength = MontageData->ReloadMontage->GetPlayLength();
	const float TargetDuration = CurrentWeapon->GetEffectiveReloadDuration();
	const float PlayRate = (MontageLength > 0.f && TargetDuration > 0.f) ? (MontageLength / TargetDuration) : 1.f;

	const float PlayedDuration = Montage_Play(MontageData->ReloadMontage, PlayRate);
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

void UShooterAnimInstance::PlayFireMontageForCurrentWeapon()
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
	if (!MontageData || !IsValid(MontageData->FireMontage))
	{
		return;
	}

	Montage_Play(MontageData->FireMontage);
}

EDominationCombatClass UShooterAnimInstance::GetCurrentCombatClass() const
{
	if (!IsValid(ShooterCharacter))
	{
		return EDominationCombatClass::Unselected;
	}

	const ADominationPlayerState* PlayerState = ShooterCharacter->GetPlayerState<ADominationPlayerState>();
	if (!IsValid(PlayerState))
	{
		return EDominationCombatClass::Unselected;
	}

	return PlayerState->GetCombatClass();
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
