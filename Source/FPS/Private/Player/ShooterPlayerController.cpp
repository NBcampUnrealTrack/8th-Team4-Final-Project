#include "Player/ShooterPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/ShooterCharacter.h"
#include "Engine/GameInstance.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GameMode/BaseRoundGameMode.h"
#include "GameMode/BaseRoundGameState.h"
#include "Input/ShooterInputConfig.h"
#include "InputActionValue.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Player/NetworkTimeSyncComponent.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/GameplayTags.h"
#include "UI/Loading/LoadingScreenWidget.h"
#include "Weapon/WeaponTypes.h"

AShooterPlayerController::AShooterPlayerController()
{
	NetworkTimeSyncComponent = CreateDefaultSubobject<UNetworkTimeSyncComponent>(TEXT("NetworkTimeSyncComponent"));
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		ABaseRoundGameState* RoundGameState = nullptr;
		if (UWorld* World = GetWorld())
		{
			RoundGameState = World->GetGameState<ABaseRoundGameState>();
		}

		LoadingScreenSubsystem->BindToRoundGameState(RoundGameState);
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
	if (HasAuthority())
	{
		if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
		{
			ShooterPlayerState->SetVoiceTalking(false);
		}
	}

	StopMatchLoadReadyCheck();
	HideLoadingScreen();

	Super::EndPlay(EndPlayReason);
}

void AShooterPlayerController::NotifyLoadedWorld(FName WorldPackageName, bool bFinalDest)
{
	Super::NotifyLoadedWorld(WorldPackageName, bFinalDest);

	if (!bFinalDest)
	{
		return;
	}

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		ABaseRoundGameState* RoundGameState = nullptr;
		if (UWorld* World = GetWorld())
		{
			RoundGameState = World->GetGameState<ABaseRoundGameState>();
		}

		LoadingScreenSubsystem->BindToRoundGameState(RoundGameState);
		LoadingScreenSubsystem->SetLoadingProgress(0.5f);
	}

	StartMatchLoadReadyCheck();
}

void AShooterPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	if (IsLocalController() && NetworkTimeSyncComponent)
	{
		NetworkTimeSyncComponent->StartTimeSync();
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
	if (InputConfig->PushToTalkAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->PushToTalkAction, ETriggerEvent::Started, this, &ThisClass::Input_PushToTalkPressed);
		EnhancedInputComponent->BindAction(InputConfig->PushToTalkAction, ETriggerEvent::Completed, this, &ThisClass::Input_PushToTalkReleased);
		EnhancedInputComponent->BindAction(InputConfig->PushToTalkAction, ETriggerEvent::Canceled, this, &ThisClass::Input_PushToTalkReleased);
	}

	for (const FShooterAbilityInputAction& AbilityInput : InputConfig->AbilityInputActions)
	{
		if (!IsValid(AbilityInput.InputAction) || !AbilityInput.InputTag.IsValid())
		{
			continue;
		}

		ETriggerEvent PressEvent = ETriggerEvent::Started;
		if (AbilityInput.bTriggerWhileHeld)
		{
			PressEvent = ETriggerEvent::Triggered;
		}

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

void AShooterPlayerController::StartVoiceTalking()
{
	if (HasAuthority())
	{
		if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
		{
			ShooterPlayerState->SetVoiceTalking(true);
		}
		return;
	}

	ServerSetVoiceTalking(true);
}

void AShooterPlayerController::StopVoiceTalking()
{
	if (HasAuthority())
	{
		if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
		{
			ShooterPlayerState->SetVoiceTalking(false);
		}
		return;
	}

	ServerSetVoiceTalking(false);
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

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		ABaseRoundGameState* RoundGameState = nullptr;
		if (UWorld* World = GetWorld())
		{
			RoundGameState = World->GetGameState<ABaseRoundGameState>();
		}

		LoadingScreenSubsystem->BindToRoundGameState(RoundGameState);
		LoadingScreenSubsystem->ShowLoadingScreen(this, LoadingScreenWidgetClass, InitialProgress);
	}

	if (UWorld* World = GetWorld())
	{
		if (World->GetGameState<ABaseRoundGameState>())
		{
			StartMatchLoadReadyCheck();
		}
	}
}

void AShooterPlayerController::SetLoadingProgress(const float Progress)
{
	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		ABaseRoundGameState* RoundGameState = nullptr;
		if (UWorld* World = GetWorld())
		{
			RoundGameState = World->GetGameState<ABaseRoundGameState>();
		}

		LoadingScreenSubsystem->BindToRoundGameState(RoundGameState);
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

void AShooterPlayerController::ServerSetVoiceTalking_Implementation(bool bNewVoiceTalking)
{
	if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
	{
		ShooterPlayerState->SetVoiceTalking(bNewVoiceTalking);
	}
}

void AShooterPlayerController::ServerNotifyLoadedIntoMatch_Implementation()
{
	if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
	{
		ShooterPlayerState->SetLoadedIntoMatch(true);
	}

	ABaseRoundGameMode* RoundGameMode = nullptr;
	if (UWorld* World = GetWorld())
	{
		RoundGameMode = World->GetAuthGameMode<ABaseRoundGameMode>();
	}

	if (RoundGameMode)
	{
		RoundGameMode->NotifyPlayerReadyForGameFlow(this);
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
	if (bNotifiedLoadedIntoMatch)
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

void AShooterPlayerController::Input_PrimaryWeaponPressed()
{
	if (!CanProcessActionInput())
	{
		return;
	}

	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleWeaponSlotPressed(EWeaponSlot::Primary);
	}
}

void AShooterPlayerController::Input_SecondaryWeaponPressed()
{
	if (!CanProcessActionInput())
	{
		return;
	}

	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleWeaponSlotPressed(EWeaponSlot::Secondary);
	}
}

void AShooterPlayerController::Input_PushToTalkPressed()
{
	StartVoiceTalking();
}

void AShooterPlayerController::Input_PushToTalkReleased()
{
	StopVoiceTalking();
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
	if (IsValid(ShooterPlayerState))
	{
		return ShooterPlayerState->GetRogueAbilitySystemComponent();
	}

	return nullptr;
}

ULoadingScreenSubsystem* AShooterPlayerController::GetLoadingScreenSubsystem() const
{
	if (!IsLocalController())
	{
		return nullptr;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (IsValid(GameInstance))
	{
		return GameInstance->GetSubsystem<ULoadingScreenSubsystem>();
	}

	return nullptr;
}
