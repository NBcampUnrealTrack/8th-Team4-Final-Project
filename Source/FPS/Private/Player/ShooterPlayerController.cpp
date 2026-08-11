#include "Player/ShooterPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/ShooterCharacter.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "Input/ShooterInputConfig.h"
#include "InputActionValue.h"
#include "Player/ShooterPlayerState.h"

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

	BindAbilityActions(EnhancedInputComponent);
}

void AShooterPlayerController::BindAbilityActions(UEnhancedInputComponent* EnhancedInputComponent)
{
	for (int32 Index = 0; Index < InputConfig->AbilityActions.Num(); ++Index)
	{
		const FShooterAbilityInputAction& Entry = InputConfig->AbilityActions[Index];

		if (!Entry.InputAction || !Entry.InputTag.IsValid())
		{
			continue;
		}

		EnhancedInputComponent->BindAction(Entry.InputAction, ETriggerEvent::Started, this,
			&ThisClass::Input_AbilityPressed, Entry.InputTag);
		EnhancedInputComponent->BindAction(Entry.InputAction, ETriggerEvent::Completed, this,
			&ThisClass::Input_AbilityReleased, Entry.InputTag);
		EnhancedInputComponent->BindAction(Entry.InputAction, ETriggerEvent::Canceled, this,
			&ThisClass::Input_AbilityReleased, Entry.InputTag);
	}
}

URogueAbilitySystemComponent* AShooterPlayerController::GetRogueAbilitySystem() const
{
	const AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>();
	if (!ShooterPlayerState)
	{
		return nullptr;
	}

	return ShooterPlayerState->GetRogueAbilitySystemComponent();
}

void AShooterPlayerController::Input_AbilityPressed(FGameplayTag InputTag)
{
	if (URogueAbilitySystemComponent* AbilitySystem = GetRogueAbilitySystem())
	{
		AbilitySystem->AbilityInputTagPressed(InputTag);
	}
}

void AShooterPlayerController::Input_AbilityReleased(FGameplayTag InputTag)
{
	if (URogueAbilitySystemComponent* AbilitySystem = GetRogueAbilitySystem())
	{
		AbilitySystem->AbilityInputTagReleased(InputTag);
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
