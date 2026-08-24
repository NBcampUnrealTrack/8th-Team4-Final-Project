#include "Player/ShooterPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/ShooterCharacter.h"
#include "Engine/GameInstance.h"
#include "GameMode/BaseRoundGameState.h"
#include "GameMode/LobbyGameMode.h"
#include "Input/ShooterInputConfig.h"
#include "InputActionValue.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Player/ShooterPlayerState.h"
#include "Settings/ShooterGameUserSettings.h"
#include "Tags/GameplayTags.h"
#include "UI/Loading/LoadingScreenWidget.h"
#include "Weapon/WeaponTypes.h"

AShooterPlayerController::AShooterPlayerController()
{
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingScreenSubsystem->BindToRoundGameState(
			GetWorld() ? GetWorld()->GetGameState<ABaseRoundGameState>() : nullptr);
	}

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

void AShooterPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideLoadingScreen();
	Super::EndPlay(EndPlayReason);
}

void AShooterPlayerController::NotifyLoadedWorld(const FName WorldPackageName, const bool bFinalDest)
{
	Super::NotifyLoadedWorld(WorldPackageName, bFinalDest);

	if (!bFinalDest)
	{
		return;
	}

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingScreenSubsystem->BindToRoundGameState(
			GetWorld() ? GetWorld()->GetGameState<ABaseRoundGameState>() : nullptr);
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

void AShooterPlayerController::ToggleDynamicCrosshair()
{
	UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}

	Settings->SetDynamicCrosshairEnabled(!Settings->IsDynamicCrosshairEnabled());
}

void AShooterPlayerController::SetLobbyTeam(const EGameTeam RequestedTeam)
{
	if (HasAuthority())
	{
		if (ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr)
		{
			LobbyGameMode->RequestLobbyTeam(this, RequestedTeam);
		}
		return;
	}

	ServerSetLobbyTeam(RequestedTeam);
}

void AShooterPlayerController::ShowLoadingScreen(const float InitialProgress)
{
	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingScreenSubsystem->BindToRoundGameState(
			GetWorld() ? GetWorld()->GetGameState<ABaseRoundGameState>() : nullptr);
		LoadingScreenSubsystem->ShowLoadingScreen(this, LoadingScreenWidgetClass, InitialProgress);
	}
}

void AShooterPlayerController::SetLoadingProgress(const float Progress)
{
	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		// Seamless travel 직후 GameState 복제가 늦은 경우 UI 준비 시점에 다시 바인딩한다.
		LoadingScreenSubsystem->BindToRoundGameState(
			GetWorld() ? GetWorld()->GetGameState<ABaseRoundGameState>() : nullptr);
		LoadingScreenSubsystem->SetLoadingProgress(Progress);
	}
}

void AShooterPlayerController::HideLoadingScreen()
{
	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingScreenSubsystem->HideLoadingScreen();
	}
}

void AShooterPlayerController::ClientShowLoadingScreen_Implementation(const float InitialProgress)
{
	ShowLoadingScreen(InitialProgress);
}

void AShooterPlayerController::ClientSetLoadingProgress_Implementation(const float Progress)
{
	SetLoadingProgress(Progress);
}

void AShooterPlayerController::ClientHideLoadingScreen_Implementation()
{
	HideLoadingScreen();
}

void AShooterPlayerController::ServerSetLobbyReady_Implementation(bool bNewLobbyReady)
{
	if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
	{
		ShooterPlayerState->SetLobbyReady(bNewLobbyReady);
	}
}

void AShooterPlayerController::ServerSetLobbyTeam_Implementation(const EGameTeam RequestedTeam)
{
	// 정원 검사와 실제 팀 반영은 서버 권한인 GameMode 가 담당
	if (ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr)
	{
		LobbyGameMode->RequestLobbyTeam(this, RequestedTeam);
	}
}

bool AShooterPlayerController::CanProcessGameplayInput() const
{
	const URogueAbilitySystemComponent* AbilitySystem = GetRogueAbilitySystemComponent();
	return !IsValid(AbilitySystem)
		|| !AbilitySystem->HasMatchingGameplayTag(RogueTags::State_Gameplay_InputBlocked);
}

bool AShooterPlayerController::CanProcessMovementInput() const
{
	return CanProcessGameplayInput() && !HasDataJackInteractionInputBlock();
}

bool AShooterPlayerController::CanProcessActionInput() const
{
	return CanProcessGameplayInput() && !HasDataJackInteractionInputBlock();
}

bool AShooterPlayerController::HasDataJackInteractionInputBlock() const
{
	const URogueAbilitySystemComponent* AbilitySystem = GetRogueAbilitySystemComponent();
	return IsValid(AbilitySystem)
		&& AbilitySystem->HasMatchingGameplayTag(RogueTags::State_Combat_DataJack_Interacting);
}

void AShooterPlayerController::Input_Move(const FInputActionValue& Value)
{
	if (!CanProcessMovementInput())
	{
		return;
	}

	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleMoveInput(Value);
	}
}

void AShooterPlayerController::Input_Look(const FInputActionValue& Value)
{
	if (!CanProcessGameplayInput())
	{
		return;
	}

	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleLookInput(Value);
	}
}

void AShooterPlayerController::Input_JumpPressed()
{
	if (!CanProcessMovementInput())
	{
		return;
	}

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
	if (!CanProcessMovementInput())
	{
		return;
	}

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

void AShooterPlayerController::Input_AbilityPressed(const FInputActionValue&, const FGameplayTag InputTag)
{
	if (!CanProcessActionInput())
	{
		return;
	}

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

ULoadingScreenSubsystem* AShooterPlayerController::GetLoadingScreenSubsystem() const
{
	if (!IsLocalController())
	{
		return nullptr;
	}

	UGameInstance* GameInstance = GetGameInstance();
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<ULoadingScreenSubsystem>() : nullptr;
}
