#include "Player/ShooterPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/ShooterCharacter.h"
#include "Input/ShooterInputConfig.h"
#include "InputActionValue.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "Player/ShooterPlayerState.h"
#include "Weapon/WeaponTypes.h"

AShooterPlayerController::AShooterPlayerController()
{
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController() || !InputConfig || !InputConfig->DefaultMappingContext)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (InputSubsystem)
	{
		InputSubsystem->AddMappingContext(InputConfig->DefaultMappingContext, InputConfig->MappingPriority);
	}
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent || !InputConfig)
	{
		return;
	}

	if (InputConfig->MoveAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	}
	if (InputConfig->LookAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->LookAction, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	}
	if (InputConfig->JumpAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->JumpAction, ETriggerEvent::Started, this, &ThisClass::Input_JumpPressed);
		EnhancedInputComponent->BindAction(InputConfig->JumpAction, ETriggerEvent::Completed, this, &ThisClass::Input_JumpReleased);
	}
	if (InputConfig->SprintAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->SprintAction, ETriggerEvent::Started, this, &ThisClass::Input_SprintPressed);
		EnhancedInputComponent->BindAction(InputConfig->SprintAction, ETriggerEvent::Completed, this, &ThisClass::Input_SprintReleased);
	}
	if (InputConfig->PrimaryWeaponAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->PrimaryWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_PrimaryWeaponPressed);
	}
	if (InputConfig->SecondaryWeaponAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->SecondaryWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_SecondaryWeaponPressed);
	}

	for (const FShooterAbilityInputAction& AbilityInput : InputConfig->AbilityInputActions)
	{
		if (!IsValid(AbilityInput.InputAction) || !AbilityInput.InputTag.IsValid())
		{
			continue;
		}

		const ETriggerEvent PressEvent = AbilityInput.bTriggerWhileHeld
			? ETriggerEvent::Triggered
			: ETriggerEvent::Started;

		EnhancedInputComponent->BindAction(
			AbilityInput.InputAction, PressEvent, this, &ThisClass::Input_AbilityPressed, AbilityInput.InputTag);
		EnhancedInputComponent->BindAction(
			AbilityInput.InputAction, ETriggerEvent::Completed, this, &ThisClass::Input_AbilityReleased, AbilityInput.InputTag);
		EnhancedInputComponent->BindAction(
			AbilityInput.InputAction, ETriggerEvent::Canceled, this, &ThisClass::Input_AbilityReleased, AbilityInput.InputTag);
	}
}

void AShooterPlayerController::Input_Move(const FInputActionValue& Value)
{
	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleMoveInput(Value);
	}
}

void AShooterPlayerController::Input_Look(const FInputActionValue& Value)
{
	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleLookInput(Value);
	}
}

void AShooterPlayerController::Input_JumpPressed()
{
	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleJumpPressed();
	}
}

void AShooterPlayerController::Input_JumpReleased()
{
	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleJumpReleased();
	}
}

void AShooterPlayerController::Input_SprintPressed()
{
	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleSprintPressed();
	}
}

void AShooterPlayerController::Input_SprintReleased()
{
	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleSprintReleased();
	}
}

void AShooterPlayerController::Input_PrimaryWeaponPressed()
{
	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleWeaponSlotPressed(EWeaponSlot::Primary);
	}
}

void AShooterPlayerController::Input_SecondaryWeaponPressed()
{
	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleWeaponSlotPressed(EWeaponSlot::Secondary);
	}
}

void AShooterPlayerController::Input_AbilityPressed(const FInputActionValue&, const FGameplayTag InputTag)
{
	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void AShooterPlayerController::Input_AbilityReleased(const FInputActionValue&, const FGameplayTag InputTag)
{
	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

URogueAbilitySystemComponent* AShooterPlayerController::GetRogueAbilitySystemComponent() const
{
	const AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>();
	return IsValid(ShooterPlayerState) ? ShooterPlayerState->GetRogueAbilitySystemComponent() : nullptr;
}
