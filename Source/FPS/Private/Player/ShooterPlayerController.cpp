#include "Player/ShooterPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/ShooterCharacter.h"
#include "Input/ShooterInputConfig.h"
#include "InputActionValue.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GameFlow/Round/BaseRoundGameMode.h"
#include "Player/ShooterPlayerState.h"
#include "TimerManager.h"
#include "UI/LoadingScreenWidget.h"
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

void AShooterPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMatchLoadReadyCheck();
	HideLoadingScreen();

	Super::EndPlay(EndPlayReason);
}

void AShooterPlayerController::NotifyLoadedWorld(FName WorldPackageName, bool bFinalDest)
{
	Super::NotifyLoadedWorld(WorldPackageName, bFinalDest);

	if (bFinalDest && LoadingScreenWidget)
	{
		SetLoadingProgress(0.7f);
		StartMatchLoadReadyCheck();
	}
}

void AShooterPlayerController::SetLobbyReady(bool bNewLobbyReady)
{
	if (HasAuthority())
	{
		if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
		{
			ShooterPlayerState->SetLobbyReady(bNewLobbyReady);
		}
		return;
	}

	ServerSetLobbyReady(bNewLobbyReady);
}

void AShooterPlayerController::ShowLoadingScreen(const float InitialProgress)
{
	if (!IsLocalController())
	{
		return;
	}

	if (InitialProgress <= 0.3f)
	{
		bNotifiedLoadedIntoMatch = false;
	}

	if (!LoadingScreenWidget && LoadingScreenWidgetClass)
	{
		LoadingScreenWidget = CreateWidget<ULoadingScreenWidget>(this, LoadingScreenWidgetClass);
		if (LoadingScreenWidget)
		{
			LoadingScreenWidget->AddToViewport(10000);
		}
	}

	SetLoadingProgress(InitialProgress);
}

void AShooterPlayerController::SetLoadingProgress(const float Progress)
{
	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->SetLoadingProgress(Progress);
	}
}

void AShooterPlayerController::HideLoadingScreen()
{
	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget = nullptr;
	}
}

void AShooterPlayerController::ClientShowLoadingScreen_Implementation(float InitialProgress)
{
	ShowLoadingScreen(InitialProgress);
}

void AShooterPlayerController::ClientSetLoadingProgress_Implementation(float Progress)
{
	SetLoadingProgress(Progress);
}

void AShooterPlayerController::ClientHideLoadingScreen_Implementation()
{
	HideLoadingScreen();
}

void AShooterPlayerController::ServerNotifyLoadedIntoMatch_Implementation()
{
	if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
	{
		ShooterPlayerState->SetLoadedIntoMatch(true);
	}

	if (ABaseRoundGameMode* RoundGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABaseRoundGameMode>() : nullptr)
	{
		RoundGameMode->NotifyPlayerLoadedIntoMatch(this);
	}
}

void AShooterPlayerController::StartMatchLoadReadyCheck()
{
	if (!IsLocalController() || bNotifiedLoadedIntoMatch)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		MatchLoadReadyCheckTimerHandle,
		this,
		&ThisClass::CheckMatchLoadReady,
		0.2f,
		true);
}

void AShooterPlayerController::StopMatchLoadReadyCheck()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchLoadReadyCheckTimerHandle);
	}
}

void AShooterPlayerController::CheckMatchLoadReady()
{
	if (bNotifiedLoadedIntoMatch || !LoadingScreenWidget)
	{
		StopMatchLoadReadyCheck();
		return;
	}

	if (!GetPlayerState<AShooterPlayerState>())
	{
		return;
	}

	SetLoadingProgress(0.9f);
	bNotifiedLoadedIntoMatch = true;
	StopMatchLoadReadyCheck();

	if (HasAuthority())
	{
		ServerNotifyLoadedIntoMatch_Implementation();
	}
	else
	{
		ServerNotifyLoadedIntoMatch();
	}
}

void AShooterPlayerController::ServerSetLobbyReady_Implementation(bool bNewLobbyReady)
{
	if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
	{
		ShooterPlayerState->SetLobbyReady(bNewLobbyReady);
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
