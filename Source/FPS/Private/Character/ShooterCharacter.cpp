// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/ShooterCharacter.h"
#include "Customization/GameplayAppearanceComponent.h"
#include "Customization/FirstPersonAppearanceComponent.h"
#include "Combat/RogueDeathPresentation.h"

#include "AbilitySystemComponent.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/ShooterMovementCameraShake.h"
#include "Character/FootstepComponent.h"
#include "Character/ShooterMovementComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/LagCompensationComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DominationSandstormComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/RogueAugmentAttributeSet.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameMode/ShooterGameMode.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/DominationPlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "Perception/AISense_Hearing.h"
#include "Settings/ShooterAudioSettings.h"
#include "Sound/SoundConcurrency.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterCharacter, Log, All);

namespace
{
	// MM_CharacterOutliner 의 If 노드가 비교하는 상수와 같아야 한다.
	constexpr int32 TeamOutlineStencilAlly = 1;
	constexpr int32 TeamOutlineStencilEnemy = 2;
	constexpr int32 TeamOutlineStencilEnemyRevealed = 3;

}

AShooterCharacter::AShooterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShooterMovementComponent>(
		  ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	JumpCameraShake = UShooterJumpCameraShake::StaticClass();
	LandingCameraShake = UShooterLandingCameraShake::StaticClass();
	SprintCameraShake = UShooterSprintCameraShake::StaticClass();

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 0.f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraLag = false;
	SpringArm->bEnableCameraRotationLag = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(SpringArm);
	FirstPersonCamera->bUsePawnControlRotation = false;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(FirstPersonCamera);
	Mesh1P->bOnlyOwnerSee = true;
	Mesh1P->bOwnerNoSee = false;
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->bReceivesDecals = false;
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	GetMesh()->bOnlyOwnerSee = false;
	GetMesh()->bOwnerNoSee = true;
	GetMesh()->bReceivesDecals = false;

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
	Combat->SetIsReplicated(true);

	LagCompensation = CreateDefaultSubobject<ULagCompensationComponent>(TEXT("LagCompensation"));

	Footstep = CreateDefaultSubobject<UFootstepComponent>(TEXT("Footstep"));
    GameplayAppearance = CreateDefaultSubobject<UGameplayAppearanceComponent>(TEXT("GameplayAppearance"));
    FirstPersonAppearance = CreateDefaultSubobject<UFirstPersonAppearanceComponent>(TEXT("FirstPersonAppearance"));
}

void AShooterCharacter::BeginPlay()
{
	if (HasAuthority() && !PawnLifeId.IsValid())
	{
		PawnLifeId = FGuid::NewGuid();
	}
	Super::BeginPlay();
	StandingCameraRelativeLocation = FirstPersonCamera->GetRelativeLocation();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GravityScale *= GravityScaleMultiplier;
	}

	OnCharacterMovementUpdated.AddDynamic(this, &AShooterCharacter::HandleCharacterMovementUpdated);
	RefreshTeamOutline();

	if (HasAuthority())
	{
		InitializeDefaultWeaponLoadout();
	}
}

void AShooterCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bInterpolatingCrouchCamera || !IsLocallyControlled())
	{
		SetActorTickEnabled(false);
		return;
	}

	const FVector TargetLocation = SpringArm->GetComponentTransform().TransformPosition(StandingCameraRelativeLocation);
	const FVector NewLocation = FMath::VInterpTo(
		FirstPersonCamera->GetComponentLocation(),
		TargetLocation,
		DeltaSeconds,
		12.0f);
	FirstPersonCamera->SetWorldLocation(NewLocation);

	if (NewLocation.Equals(TargetLocation, 0.1f))
	{
		FirstPersonCamera->SetWorldLocation(TargetLocation);
		bInterpolatingCrouchCamera = false;
		SetActorTickEnabled(false);
	}
}

void AShooterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMovementCameraShake();
	StopLocomotionCameraShake(true);
	UnsubscribeTags();

	Super::EndPlay(EndPlayReason);
}

void AShooterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
    GameplayAppearance->RefreshFromPlayerState();
    FirstPersonAppearance->RefreshFromPlayerState();

	UpdateTagSubscriptions();
	RefreshTeamOutline();

	// 부활 직후 무기 액터가 캐릭터보다 먼저 만들어지고 빙의가 뒤따르면, 무기 장착 시점에
	// 캐릭터 메쉬/오너 참조가 아직 준비되지 않아 부착·표시가 누락될 수 있다. 여기서 다시 확정한다.
	if (IsValid(Combat))
	{
		Combat->RefreshEquippedWeaponPresentation();
	}
}

void AShooterCharacter::UnPossessed()
{
	StopMovementCameraShake();
	StopLocomotionCameraShake(true);
    Super::UnPossessed();
    FirstPersonAppearance->RefreshFromPlayerState();
    UpdateTagSubscriptions();
}

void AShooterCharacter::OnRep_Controller()
{
	StopMovementCameraShake();
	StopLocomotionCameraShake(true);
    Super::OnRep_Controller();
    FirstPersonAppearance->RefreshFromPlayerState();
}

void AShooterCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
    GameplayAppearance->RefreshFromPlayerState();
    FirstPersonAppearance->RefreshFromPlayerState();

	UpdateTagSubscriptions();
	RefreshTeamOutline();

	// 클라이언트에서도 PlayerState/오너 복제가 무기 복제보다 늦게 도착하는 경우를 대비해
	// 장착 무기 표현을 다시 적용한다(멱등).
	if (IsValid(Combat))
	{
		Combat->RefreshEquippedWeaponPresentation();
	}
}

void AShooterCharacter::RefreshTeamOutline(const bool bForceRefresh)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* ViewerController = IsValid(GameInstance)
		? GameInstance->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	const ABaseTeamPlayerState* ViewerPlayerState = IsValid(ViewerController) && ViewerController->IsLocalController()
		? ViewerController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	const ABaseTeamPlayerState* OwnerPlayerState = GetPlayerState<ABaseTeamPlayerState>();
	bool bOutlineVisible = IsValid(ViewerPlayerState) && IsValid(OwnerPlayerState)
		&& ViewerPlayerState != OwnerPlayerState
		&& ViewerPlayerState->GetTeam() != EGameTeam::None
		&& OwnerPlayerState->GetTeam() != EGameTeam::None;

	int32 StencilValue = TeamOutlineStencilAlly;
	if (bOutlineVisible && ViewerPlayerState->GetTeam() != OwnerPlayerState->GetTeam())
	{
		const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
		const bool bRevealed = IsValid(AbilitySystem)
			&& AbilitySystem->HasMatchingGameplayTag(RogueTags::State_Combat_Revealed);
		StencilValue = bRevealed ? TeamOutlineStencilEnemyRevealed : TeamOutlineStencilEnemy;

		const UDominationSandstormComponent* Sandstorm = ViewerController->FindComponentByClass<UDominationSandstormComponent>();
		bOutlineVisible = !IsValid(Sandstorm) || Sandstorm->IsEnemyOutlineInRange(*this);
	}

	const int32 OutlineState = bOutlineVisible ? StencilValue : 0;
	if (!bForceRefresh && CachedTeamOutlineState == OutlineState)
	{
		return;
	}
	USkeletalMeshComponent* Mesh3P = GetMesh();
	if (!ensure(IsValid(Mesh3P)))
	{
		return;
	}

	TArray<USceneComponent*> OutlineTargets;
	Mesh3P->GetChildrenComponents(true, OutlineTargets);
	OutlineTargets.Add(Mesh3P);
	for (USceneComponent* Target : OutlineTargets)
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Target))
		{
			const bool bRenderOutline = bOutlineVisible && !Primitive->ComponentHasTag(TEXT("AppearanceNoOutline"));
			if (Primitive->CustomDepthStencilValue != StencilValue)
			{
				Primitive->SetCustomDepthStencilValue(StencilValue);
			}
			if (Primitive->bRenderCustomDepth != bRenderOutline)
			{
				Primitive->SetRenderCustomDepth(bRenderOutline);
			}
		}
	}
	CachedTeamOutlineState = OutlineState;
}

void AShooterCharacter::RefreshTeamOutlineForAll(UWorld* World)
{
	if (!IsValid(World))
	{
		return;
	}

	for (TActorIterator<AShooterCharacter> Iterator(World); Iterator; ++Iterator)
	{
		if (AShooterCharacter* Character = *Iterator)
		{
			Character->RefreshTeamOutline();
		}
	}
}

void AShooterCharacter::UpdateTagSubscriptions()
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	if (SubscribedAbilitySystem.Get() == AbilitySystem)
	{
		return;
	}

	UnsubscribeTags();

	if (!IsValid(AbilitySystem))
	{
		return;
	}

	SubscribedAbilitySystem = AbilitySystem;

	RevealTagEventHandle = AbilitySystem->RegisterGameplayTagEvent(
		RogueTags::State_Combat_Revealed,
		EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleRevealTagChanged);

	UtilityHeldEventHandle = AbilitySystem->RegisterGameplayTagEvent(
		RogueTags::State_Combat_UtilityHeld,
		EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleUtilityHeldChanged);

	if (IsValid(Combat))
	{
		Combat->SetPresentationSuppressed(
			AbilitySystem->HasMatchingGameplayTag(RogueTags::State_Combat_UtilityHeld));
	}

	StaminaChangedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetStaminaAttribute()).AddUObject(this, &ThisClass::HandleStaminaChanged);

	UpdateSprintBlocked(AbilitySystem->GetNumericAttribute(URogueVitalAttributeSet::GetStaminaAttribute()));

	MagazineMultiplierChangedHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(
		URogueAugmentAttributeSet::GetMagazineSizeMultiplierAttribute()).AddUObject(this, &ThisClass::HandleMagazineMultiplierChanged);

	HandleMagazineMultiplierChanged(FOnAttributeChangeData());

	AimingTagEventHandle = AbilitySystem->RegisterGameplayTagEvent(
		RogueTags::State_Combat_IsAiming,
		EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleAimingTagChanged);

	WeaponFiredEventHandle = AbilitySystem->AddGameplayEventTagContainerDelegate(
		FGameplayTagContainer(RogueTags::Event_Weapon_Fired),
		FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleWeaponFired));
}

void AShooterCharacter::UnsubscribeTags()
{
	if (UAbilitySystemComponent* AbilitySystem = SubscribedAbilitySystem.Get())
	{
		AbilitySystem->RegisterGameplayTagEvent(
			RogueTags::State_Combat_Revealed,
			EGameplayTagEventType::NewOrRemoved).Remove(RevealTagEventHandle);

		AbilitySystem->RegisterGameplayTagEvent(
			RogueTags::State_Combat_UtilityHeld,
			EGameplayTagEventType::NewOrRemoved).Remove(UtilityHeldEventHandle);

		AbilitySystem->GetGameplayAttributeValueChangeDelegate(
			URogueVitalAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedHandle);

		AbilitySystem->GetGameplayAttributeValueChangeDelegate(
			URogueAugmentAttributeSet::GetMagazineSizeMultiplierAttribute()).Remove(MagazineMultiplierChangedHandle);

		AbilitySystem->RegisterGameplayTagEvent(
			RogueTags::State_Combat_IsAiming,
			EGameplayTagEventType::NewOrRemoved).Remove(AimingTagEventHandle);

		AbilitySystem->RemoveGameplayEventTagContainerDelegate(
			FGameplayTagContainer(RogueTags::Event_Weapon_Fired), WeaponFiredEventHandle);
	}

	RevealTagEventHandle.Reset();
	UtilityHeldEventHandle.Reset();
	StaminaChangedHandle.Reset();
	MagazineMultiplierChangedHandle.Reset();
	AimingTagEventHandle.Reset();
	WeaponFiredEventHandle.Reset();
	SubscribedAbilitySystem.Reset();
}

void AShooterCharacter::HandleRevealTagChanged(const FGameplayTag, const int32)
{
	RefreshTeamOutline();
}

void AShooterCharacter::HandleAimingTagChanged(const FGameplayTag, const int32 NewCount)
{
	if (NewCount > 0)
	{
		HandleSprintReleased();
	}
}

void AShooterCharacter::HandleWeaponFired(FGameplayTag, const FGameplayEventData*)
{
	HandleSprintReleased();
	if (HasAuthority())
	{
		UAISense_Hearing::ReportNoiseEvent(
			this, GetActorLocation(), 2.0f, this, 3500.0f, TEXT("Gunshot"));
	}
}

void AShooterCharacter::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	UpdateSprintBlocked(Data.NewValue);
}

void AShooterCharacter::HandleMagazineMultiplierChanged(const FOnAttributeChangeData&)
{
	for (const TWeakObjectPtr<AWeaponBase>& Weapon : AugmentedWeapons)
	{
		ApplyAugmentMagazine(Weapon.Get());
	}
}

void AShooterCharacter::OnWeaponSpawned_Implementation(AWeaponBase* Weapon)
{
	AugmentedWeapons.AddUnique(Weapon);
	ApplyAugmentMagazine(Weapon);
}

void AShooterCharacter::ApplyAugmentMagazine(AWeaponBase* Weapon)
{
	if (!IsValid(Weapon) || !Weapon->UsesAmmo())
	{
		return;
	}

	Weapon->ResizeMagazine(
		URogueAugmentStatResolver::GetFinalMagazineSize(this, Weapon->GetMagazineSize()));

	// 예비 탄약은 복제되므로 서버만
	if (HasAuthority())
	{
		Weapon->ResizeReserve(
			URogueAugmentStatResolver::GetFinalMagazineSize(this, Weapon->GetMaxReserveAmmo()));
	}
}

void AShooterCharacter::UpdateSprintBlocked(const float Stamina)
{
	if (UShooterMovementComponent* ShooterMovement = Cast<UShooterMovementComponent>(GetCharacterMovement()))
	{
		if (Stamina <= 0.f && ShooterMovement->IsSprinting() && IsLocallyControlled())
		{
			PlayExhaustedBreathSound();
		}

		ShooterMovement->SetSprintBlocked(Stamina <= 0.f);
	}
}

void AShooterCharacter::PlayExhaustedBreathSound() const
{
	const UShooterAudioSettings* AudioSettings = GetDefault<UShooterAudioSettings>();
	if (!AudioSettings)
	{
		return;
	}

	const ADominationPlayerState* DominationState = GetPlayerState<ADominationPlayerState>();
	const EDominationCombatClass CombatClass = IsValid(DominationState)
		? DominationState->GetCombatClass()
		: EDominationCombatClass::Unselected;

	const TArray<TSoftObjectPtr<USoundBase>>& Pool = AudioSettings->ResolveExhaustedBreathSoundPool(CombatClass);
	if (Pool.IsEmpty())
	{
		return;
	}

	if (USoundBase* Sound = Pool[FMath::RandHelper(Pool.Num())].LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Sound, 1.f, 1.f, 0.f,
			AudioSettings->ExhaustedBreathConcurrency.LoadSynchronous());
	}
}

void AShooterCharacter::HandleUtilityHeldChanged(const FGameplayTag, const int32 NewCount)
{
	if (!ensure(IsValid(Combat)))
	{
		return;
	}

	const bool bHeld = NewCount > 0;
	Combat->SetPresentationSuppressed(bHeld);

	if (!bHeld && HasAuthority())
	{
		Combat->ReEquipCurrentWeapon();
	}
}

bool AShooterCharacter::InitializeDefaultWeaponLoadout()
{
	if (!HasAuthority() || !IsValid(Combat) || !DefaultPrimaryWeaponClass)
	{
		return false;
	}

	if (IsValid(Combat->GetWeaponInSlot(EWeaponSlot::Primary)))
	{
		return true;
	}

	if (!Combat->InitializeWeaponLoadout(DefaultPrimaryWeaponClass, DefaultSecondaryWeaponClass, DefaultMeleeWeaponClass))
	{
		UE_LOG(
			LogShooterCharacter,
			Error,
			TEXT("[ShooterCharacter] %s: 기본 무기 로드아웃 초기화 실패. Primary=%s Secondary=%s Melee=%s"),
			*GetNameSafe(this),
			*GetNameSafe(DefaultPrimaryWeaponClass.Get()),
			*GetNameSafe(DefaultSecondaryWeaponClass.Get()),
			*GetNameSafe(DefaultMeleeWeaponClass.Get()));
		return false;
	}

	UE_LOG(
		LogShooterCharacter,
		Log,
		TEXT("[ShooterCharacter] %s: 기본 무기 로드아웃 준비 완료. Primary=%s Secondary=%s Melee=%s"),
		*GetNameSafe(this),
			*GetNameSafe(Combat->GetWeaponInSlot(EWeaponSlot::Primary)),
			*GetNameSafe(Combat->GetWeaponInSlot(EWeaponSlot::Secondary)),
			*GetNameSafe(Combat->GetWeaponInSlot(EWeaponSlot::Melee)));

	return true;
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AShooterCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!ensure(IsValid(Movement)))
	{
		return;
	}

	const bool bWasInAir = (PrevMovementMode == MOVE_Falling);
	const bool bIsInAir  = (Movement->MovementMode == MOVE_Falling);
	if (bWasInAir == bIsInAir)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		AbilitySystem->SetLooseGameplayTagCount(RogueTags::State_Movement_InAir, bIsInAir ? 1 : 0);
	}
}

void AShooterCharacter::Landed(const FHitResult& Hit)
{
	// Capture impact velocity before Super dispatches Blueprint landing events.
	const UPrimitiveComponent* Surface = Hit.GetComponent();
	const FVector SurfaceVelocity = IsValid(Surface) ? Surface->GetComponentVelocity() : FVector::ZeroVector;
	const float ImpactSpeed = FMath::Max(0.f,
		static_cast<float>(-FVector::DotProduct(GetVelocity() - SurfaceVelocity, Hit.ImpactNormal)));
	Super::Landed(Hit);

	const float MinSpeed = FMath::Max(0.f, MinLandingImpactSpeed);
	if (ImpactSpeed > MinSpeed)
	{
		const float SpeedRange = FMath::Max(1.f, FullLandingImpactSpeed - MinSpeed);
		const float ImpactScale = FMath::Clamp((ImpactSpeed - MinSpeed) / SpeedRange,
			0.f, FMath::Max(0.f, MaxLandingImpactScale));
		PlayMovementCameraShake(LandingCameraShake, ImpactScale * LandingCameraShakeScale);
	}
	if (HasAuthority())
	{
		UAISense_Hearing::ReportNoiseEvent(
			this, GetActorLocation(), 1.25f, this, 2200.0f, TEXT("Landing"));
	}
}

void AShooterCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	PlayMovementCameraShake(JumpCameraShake, JumpCameraShakeScale);

	if (IsValid(Footstep))
	{
		Footstep->PlayJumpSound();
	}
}

APlayerCameraManager* AShooterCharacter::GetLocalMovementCameraManager() const
{
	// Saved-move replay must not repeat a cosmetic impulse already predicted locally.
	if (bClientUpdating || bDeathStateApplied || !IsLocallyControlled())
	{
		return nullptr;
	}

	APlayerController* LocalController = Cast<APlayerController>(GetController());
	if (!IsValid(LocalController) || !LocalController->IsLocalController()
		|| LocalController->GetPawn() != this || LocalController->GetViewTarget() != this
		|| !IsValid(LocalController->PlayerCameraManager))
	{
		return nullptr;
	}

	return LocalController->PlayerCameraManager;
}

void AShooterCharacter::PlayMovementCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	if (!ShakeClass || Scale <= 0.f)
	{
		return;
	}

	APlayerCameraManager* CameraManager = GetLocalMovementCameraManager();
	if (!CameraManager)
	{
		return;
	}

	if (IsValid(Combat) && Combat->IsAiming())
	{
		Scale *= FMath::Clamp(AimingMovementCameraShakeScale, 0.f, 1.f);
	}
	if (Scale <= 0.f)
	{
		return;
	}

	// Give jump/landing priority while locomotion sway settles smoothly.
	StopLocomotionCameraShake(false);
	StopMovementCameraShake();
	MovementShakeCameraManager = CameraManager;
	ActiveMovementCameraShake = CameraManager->StartCameraShake(ShakeClass, Scale);
}

void AShooterCharacter::StopMovementCameraShake()
{
	if (APlayerCameraManager* CameraManager = MovementShakeCameraManager.Get())
	{
		if (UCameraShakeBase* Shake = ActiveMovementCameraShake.Get())
		{
			CameraManager->StopCameraShake(Shake, true);
		}
	}
	ActiveMovementCameraShake.Reset();
	MovementShakeCameraManager.Reset();
}

void AShooterCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	if (IsLocallyControlled())
	{
		FirstPersonCamera->AddWorldOffset(FVector::UpVector * ScaledHalfHeightAdjust);
		bInterpolatingCrouchCamera = true;
		SetActorTickEnabled(true);
	}

	if (IsValid(Footstep))
	{
		Footstep->PlayCrouchSound();
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		AbilitySystem->SetLooseGameplayTagCount(RogueTags::State_Movement_Crouching, 1);
	}
}

void AShooterCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	if (IsLocallyControlled())
	{
		FirstPersonCamera->AddWorldOffset(-FVector::UpVector * ScaledHalfHeightAdjust);
		bInterpolatingCrouchCamera = true;
		SetActorTickEnabled(true);
	}

	if (IsValid(Footstep))
	{
		Footstep->PlayStandSound();
	}

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		AbilitySystem->SetLooseGameplayTagCount(RogueTags::State_Movement_Crouching, 0);
	}
}

void AShooterCharacter::StopLocomotionCameraShake(bool bImmediately)
{
	UShooterSprintCameraShake* Shake = ActiveLocomotionCameraShake.Get();
	if (!bImmediately && Shake && !Shake->IsFinished())
	{
		// Keep the instance during fade-out so a quick restart preserves its phase.
		Shake->UpdateMovement(0.f, 0.f, SprintCameraStepDistance);
		return;
	}

	if (APlayerCameraManager* CameraManager = LocomotionShakeCameraManager.Get())
	{
		if (Shake)
		{
			CameraManager->StopCameraShake(Shake, true);
		}
	}
	ActiveLocomotionCameraShake.Reset();
	LocomotionShakeCameraManager.Reset();
}

void AShooterCharacter::UpdateLocomotionCameraShake()
{
	if (bClientUpdating)
	{
		return;
	}

	APlayerCameraManager* CameraManager = GetLocalMovementCameraManager();
	if (!CameraManager)
	{
		StopLocomotionCameraShake(true);
		return;
	}

	const UShooterMovementComponent* Movement = Cast<UShooterMovementComponent>(GetCharacterMovement());
	const UCameraShakeBase* MovementImpulse = ActiveMovementCameraShake.Get();
	const UShooterSprintCameraShake* ShakeDefaults = SprintCameraShake.GetDefaultObject();
	if (!ShakeDefaults || !Cast<UShooterSprintCameraShakePattern>(ShakeDefaults->GetRootShakePattern())
		|| !IsValid(Movement) || !Movement->IsMovingOnGround() || Movement->IsCrouching()
		|| (IsValid(Combat) && Combat->IsAiming())
		|| (MovementImpulse && !MovementImpulse->IsFinished()))
	{
		StopLocomotionCameraShake(false);
		return;
	}

	const float Speed = static_cast<float>(Movement->Velocity.Size2D());
	const float WalkingSpeed = Movement->GetMaxWalkingSpeed();
	if (WalkingSpeed <= UE_SMALL_NUMBER || Speed <= UE_KINDA_SMALL_NUMBER)
	{
		StopLocomotionCameraShake(false);
		return;
	}

	// Use resolved movement speeds so weapon/augment modifiers stay owned by movement.
	const float SprintSpeedRange = Movement->IsSprinting()
		? Movement->GetMaxSpeed() - WalkingSpeed : 0.f;
	const float SprintAlpha = SprintSpeedRange > UE_SMALL_NUMBER
		? FMath::Clamp((Speed - WalkingSpeed) / SprintSpeedRange, 0.f, 1.f)
		: 0.f;
	const float Intensity = FMath::Lerp(FMath::Max(0.f, WalkCameraShakeScale),
		FMath::Max(0.f, SprintCameraShakeScale), SprintAlpha)
		* FMath::Clamp(Speed / WalkingSpeed, 0.f, 1.f);
	if (Intensity <= UE_SMALL_NUMBER)
	{
		StopLocomotionCameraShake(false);
		return;
	}

	UShooterSprintCameraShake* Shake = ActiveLocomotionCameraShake.Get();
	if (!Shake || Shake->IsFinished() || LocomotionShakeCameraManager.Get() != CameraManager
		|| Shake->GetClass() != SprintCameraShake.Get())
	{
		StopLocomotionCameraShake(true);
		Shake = Cast<UShooterSprintCameraShake>(CameraManager->StartCameraShake(SprintCameraShake, 1.f));
		ActiveLocomotionCameraShake = Shake;
		LocomotionShakeCameraManager = CameraManager;
	}

	if (Shake)
	{
		const float StepDistance = FMath::Lerp(FMath::Max(1.f, WalkCameraStepDistance),
			FMath::Max(1.f, SprintCameraStepDistance), SprintAlpha);
		Shake->UpdateMovement(Speed, Intensity, StepDistance);
	}
}

void AShooterCharacter::HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	UpdateLocomotionCameraShake();

	const UShooterMovementComponent* Movement = Cast<UShooterMovementComponent>(GetCharacterMovement());
	const bool bIsSprinting = IsValid(Movement) && Movement->IsSprinting() && !Movement->IsCrouching();
	if (bIsSprinting == bSprintTagApplied)
	{
		return;
	}
	bSprintTagApplied = bIsSprinting;

	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
	{
		AbilitySystem->SetLooseGameplayTagCount(RogueTags::State_Movement_Sprinting, bIsSprinting ? 1 : 0);
	}
}

void AShooterCharacter::CheckJumpInput(float DeltaTime)
{
	if (bPressedJump)
	{
		const ACharacter* DefaultCharacter = GetClass()->GetDefaultObject<ACharacter>();
		JumpMaxCount = DefaultCharacter->JumpMaxCount + URogueAugmentStatResolver::GetExtraJumpCount(this);
	}

	Super::CheckJumpInput(DeltaTime);
}

UAbilitySystemComponent* AShooterCharacter::GetAbilitySystemComponent() const
{
	const AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>();

	return ShooterPlayerState ? ShooterPlayerState->GetAbilitySystemComponent() : nullptr;
}

USkeletalMeshComponent* AShooterCharacter::GetMesh1P_Implementation() const
{
	return Mesh1P;
}

USkeletalMeshComponent* AShooterCharacter::GetMesh3P_Implementation() const
{
	return GetMesh();
}

UCameraComponent* AShooterCharacter::GetViewCamera_Implementation() const
{
	return FirstPersonCamera;
}

FRotator AShooterCharacter::GetFixedAimRotation_Implementation() const
{
	return CalculateRawAimRotation();
}

AWeaponBase* AShooterCharacter::GetCurrentWeapon_Implementation() const
{
	return IsValid(Combat) ? Combat->GetCurrentWeapon() : nullptr;
}

UCombatComponent* AShooterCharacter::GetCombatComponent_Implementation() const
{
	return Combat;
}

void AShooterCharacter::HandleDeath_Implementation(const FRogueDeathContext& Context)
{
	if (!HasAuthority())
	{
		return;
	}

	AController* OwningController = GetController();
	UWorld* World = GetWorld();
	AShooterGameMode* ShooterGameMode = IsValid(World) ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	if (IsValid(OwningController) && IsValid(ShooterGameMode))
	{
		ShooterGameMode->HandlePlayerEliminated(OwningController, Context);
	}
}

UCombatHitZoneDefinition* AShooterCharacter::GetCombatHitZoneDefinition_Implementation() const
{
	return CombatHitZoneDefinition.Get();
}

void AShooterCharacter::PlayDeathReaction_Implementation(const FRogueDeathReaction& Reaction)
{
	if (bDeathStateApplied)
	{
		return;
	}
	bDeathStateApplied = true;
	StopMovementCameraShake();
	StopLocomotionCameraShake(true);

	if (HasAuthority())
	{
		ReplicatedDeathReaction = Reaction;
		bReplicatedDeath = true;
		ForceNetUpdate();
	}

	// Keep player-only visibility here; share only the established physical death presentation.
	Mesh1P->SetHiddenInGame(true, true);
	GetMesh()->SetOwnerNoSee(false);

	FRogueRagdollSettings Settings;
	Settings.ImpulseSpeed = DeathImpulseSpeed;
	Settings.HeadshotImpulseMultiplier = HeadshotDeathImpulseMultiplier;
	Settings.UpwardBias = DeathImpulseUpwardBias;
	Settings.ImpulseBone = DeathImpulseBoneName;
	Settings.HeadshotImpulseBone = HeadshotDeathImpulseBoneName;
	RogueDeathPresentation::StartRagdoll(*this, Reaction, Settings);
}

void AShooterCharacter::OnRep_ReplicatedDeath()
{
	if (bReplicatedDeath)
	{
		IRogueDamageableTarget::Execute_PlayDeathReaction(this, ReplicatedDeathReaction);
	}
}

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShooterCharacter, PawnLifeId);

	DOREPLIFETIME(AShooterCharacter, ReplicatedDeathReaction);
	DOREPLIFETIME(AShooterCharacter, bReplicatedDeath);
	DOREPLIFETIME_CONDITION(AShooterCharacter, ReplicatedRecoilPitchHalfDegrees, COND_SkipOwner);
}

void AShooterCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	if (HasAuthority())
	{
		const AWeaponBase* Weapon = IsValid(Combat) ? Combat->GetCurrentWeapon() : nullptr;
		const float RecoilPitch = IsValid(Weapon) ? Weapon->GetAccumulatedRecoilPitch() : 0.f;
		ReplicatedRecoilPitchHalfDegrees = static_cast<int8>(FMath::Clamp(FMath::RoundToInt(RecoilPitch * 2.f), -127, 127));
	}
}

float AShooterCharacter::GetRecoilPitchForAim() const
{
	// 서버는 무기 반동 상태를 직접 갖고 있고, 관찰자 클라이언트는 서버가 보낸 값을 쓴다.
	if (HasAuthority())
	{
		const AWeaponBase* Weapon = IsValid(Combat) ? Combat->GetCurrentWeapon() : nullptr;
		return IsValid(Weapon) ? Weapon->GetAccumulatedRecoilPitch() : 0.f;
	}
	return ReplicatedRecoilPitchHalfDegrees * 0.5f;
}

bool AShooterCharacter::HasCurrentWeapon() const
{
	return IsValid(Combat) && IsValid(Combat->GetCurrentWeapon());
}

FRotator AShooterCharacter::CalculateFixedAimRotation() const
{
	FRotator AimRotation = CalculateRawAimRotation();
	if (!IsLocallyControlled())
	{
		// 반동은 컨트롤러 Pitch에 -누적값으로 들어가 있으므로 누적값을 더해 사격 전 조준 각도로 되돌린다.
		AimRotation.Pitch += GetRecoilPitchForAim();
	}

	return AimRotation;
}

FRotator AShooterCharacter::CalculateRawAimRotation() const
{
	FRotator AimRotation = GetBaseAimRotation();
	if (AimRotation.Pitch > 90.f && !IsLocallyControlled())
	{
		const FVector2D InRange(270.f, 360.f);
		const FVector2D OutRange(-90.f, 0.f);
		AimRotation.Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AimRotation.Pitch);
	}

	return AimRotation;
}

void AShooterCharacter::HandleMoveInput(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (!Controller || MovementVector.IsNearlyZero())
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementVector.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementVector.X);
}

void AShooterCharacter::HandleLookInput(const FVector2D& LookAxisVector)
{
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void AShooterCharacter::HandleJumpPressed()
{
	Jump();
}

void AShooterCharacter::HandleJumpReleased()
{
	StopJumping();
}

void AShooterCharacter::HandleSprintPressed()
{
	// 조준 중에는 달리기 시작 불가
	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	if (IsValid(AbilitySystem) && AbilitySystem->HasMatchingGameplayTag(RogueTags::State_Combat_IsAiming))
	{
		return;
	}

	if (UShooterMovementComponent* ShooterMovement = Cast<UShooterMovementComponent>(GetCharacterMovement()))
	{
		ShooterMovement->StartSprint();
	}
}

void AShooterCharacter::HandleSprintReleased()
{
	if (UShooterMovementComponent* ShooterMovement = Cast<UShooterMovementComponent>(GetCharacterMovement()))
	{
		ShooterMovement->StopSprint();
	}
}

void AShooterCharacter::HandleCrouchPressed()
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!IsValid(Movement) || !Movement->CanCrouchInCurrentState())
	{
		return;
	}

	HandleSprintReleased();
	Crouch();
}

void AShooterCharacter::HandleCrouchReleased()
{
	UnCrouch();
}

// 무기 슬롯 전환은 UGA_SwitchWeapon(GAS 어빌리티)이 담당한다 - Fire/Reload와 동일하게
// 입력 태그 -> 어빌리티 경로를 타며, Character는 더 이상 스왑을 직접 호출하지 않는다.
