#include "Player/ShooterPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/ShooterCharacter.h"
#include "Engine/GameInstance.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GameMode/BaseRoundGameMode.h"
#include "GameMode/BaseRoundGameState.h"
#include "GameFramework/GameModeBase.h"
#include "GameMode/LobbyGameMode.h"
#include "Input/ShooterInputConfig.h"
#include "InputActionValue.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Player/NetworkTimeSyncComponent.h"
#include "Player/ShooterPlayerState.h"
#include "Settings/ShooterGameUserSettings.h"
#include "Tags/GameplayTags.h"
#include "UI/Loading/LoadingScreenWidget.h"
#include "Voice/SteamVoiceComponent.h"
#include "Voice/VoiceChatSubsystem.h"

static FString VoiceAudienceToString(const EVoiceChatAudience Audience)
{
	if (const UEnum* AudienceEnum = StaticEnum<EVoiceChatAudience>())
	{
		return AudienceEnum->GetNameStringByValue(static_cast<int64>(Audience));
	}

	return FString::FromInt(static_cast<int32>(Audience));
}

AShooterPlayerController::AShooterPlayerController()
{
	NetworkTimeSyncComponent = CreateDefaultSubobject<UNetworkTimeSyncComponent>(TEXT("NetworkTimeSyncComponent"));
	SteamVoiceComponent = CreateDefaultSubobject<USteamVoiceComponent>(TEXT("SteamVoiceComponent"));
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
	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][PCStart] PC=%s Local=%s Auth=%s VoiceComponent=%s PlayerState=%s"),
		*GetNameSafe(this),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		IsValid(SteamVoiceComponent) ? TEXT("valid") : TEXT("null"),
		*GetNameSafe(PlayerState));

	if (IsLocalController() && SteamVoiceComponent)
	{
		SteamVoiceComponent->StartCapture();
	}

	if (HasAuthority())
	{
		if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
		{
			ShooterPlayerState->SetVoiceTalking(true);
			UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][PCStart] Authority set talking true. PlayerState=%s"),
				*GetNameSafe(ShooterPlayerState));
		}
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][PCStart] Send ServerSetVoiceTalking(true). PC=%s"), *GetNameSafe(this));
	ServerSetVoiceTalking(true);
}

void AShooterPlayerController::StopVoiceTalking()
{
	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][PCStop] PC=%s Local=%s Auth=%s VoiceComponent=%s PlayerState=%s"),
		*GetNameSafe(this),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		IsValid(SteamVoiceComponent) ? TEXT("valid") : TEXT("null"),
		*GetNameSafe(PlayerState));

	if (IsLocalController() && SteamVoiceComponent)
	{
		SteamVoiceComponent->StopCapture();
	}

	if (HasAuthority())
	{
		if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
		{
			ShooterPlayerState->SetVoiceTalking(false);
			UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][PCStop] Authority set talking false. PlayerState=%s"),
				*GetNameSafe(ShooterPlayerState));
		}
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][PCStop] Send ServerSetVoiceTalking(false). PC=%s"), *GetNameSafe(this));
	ServerSetVoiceTalking(false);
}

void AShooterPlayerController::SubmitCompressedVoicePacket(const TArray<uint8>& CompressedVoiceData)
{
	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Submit] Blocked: not local. PC=%s Bytes=%d"),
			*GetNameSafe(this),
			CompressedVoiceData.Num());
		return;
	}

	if (CompressedVoiceData.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Submit] Blocked: empty packet. PC=%s"), *GetNameSafe(this));
		return;
	}

	const AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>();
	if (!IsValid(ShooterPlayerState))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Submit] Blocked: ShooterPlayerState=null. PC=%s Bytes=%d"),
			*GetNameSafe(this),
			CompressedVoiceData.Num());
		return;
	}

	if (!ShooterPlayerState->IsVoiceTalking())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Submit] Blocked: PlayerState is not talking. PC=%s PS=%s Bytes=%d"),
			*GetNameSafe(this),
			*GetNameSafe(ShooterPlayerState),
			CompressedVoiceData.Num());
		return;
	}

	EVoiceChatAudience Audience = EVoiceChatAudience::Team;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UVoiceChatSubsystem* VoiceChatSubsystem = GameInstance->GetSubsystem<UVoiceChatSubsystem>())
		{
			Audience = VoiceChatSubsystem->GetVoiceAudience();
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Submit] Sending packet. PC=%s PS=%s Bytes=%d Audience=%s Auth=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ShooterPlayerState),
		CompressedVoiceData.Num(),
		*VoiceAudienceToString(Audience),
		HasAuthority() ? TEXT("true") : TEXT("false"));

	if (HasAuthority())
	{
		ServerSubmitCompressedVoicePacket_Implementation(CompressedVoiceData, Audience);
		return;
	}

	ServerSubmitCompressedVoicePacket(CompressedVoiceData, Audience);
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

void AShooterPlayerController::RequestReturnToFrontend()
{
	if (!IsLocalController())
	{
		return;
	}

	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		GameMode->ReturnToMainMenuHost();
		return;
	}

	GetGameInstance()->ReturnToMainMenu();
}

void AShooterPlayerController::RequestQuitGame()
{
	if (IsLocalController())
	{
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
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
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ServerTalking] PC=%s PS=%s Talking=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ShooterPlayerState),
			bNewVoiceTalking ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ServerTalking] Blocked: ShooterPlayerState=null. PC=%s Talking=%s"),
			*GetNameSafe(this),
			bNewVoiceTalking ? TEXT("true") : TEXT("false"));
	}
}

void AShooterPlayerController::ServerSubmitCompressedVoicePacket_Implementation(
	const TArray<uint8>& CompressedVoiceData,
	const EVoiceChatAudience Audience)
{
	if (CompressedVoiceData.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ServerSubmit] Blocked: empty packet. PC=%s Audience=%s"),
			*GetNameSafe(this),
			*VoiceAudienceToString(Audience));
		return;
	}

	const APlayerState* SpeakerPlayerState = PlayerState;
	if (!IsValid(SpeakerPlayerState))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ServerSubmit] Blocked: speaker PlayerState=null. PC=%s Bytes=%d Audience=%s"),
			*GetNameSafe(this),
			CompressedVoiceData.Num(),
			*VoiceAudienceToString(Audience));
		return;
	}

	UVoiceChatSubsystem* VoiceChatSubsystem = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		VoiceChatSubsystem = GameInstance->GetSubsystem<UVoiceChatSubsystem>();
	}

	if (!IsValid(VoiceChatSubsystem))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ServerSubmit] Blocked: VoiceChatSubsystem=null. Speaker=%s Bytes=%d Audience=%s"),
			*GetNameSafe(SpeakerPlayerState),
			CompressedVoiceData.Num(),
			*VoiceAudienceToString(Audience));
		return;
	}

	if (!VoiceChatSubsystem->CanSendVoice(SpeakerPlayerState, Audience))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ServerSubmit] Blocked: CanSendVoice=false. Speaker=%s Bytes=%d Audience=%s"),
			*GetNameSafe(SpeakerPlayerState),
			CompressedVoiceData.Num(),
			*VoiceAudienceToString(Audience));
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ServerSubmit] Blocked: World=null. Speaker=%s Bytes=%d Audience=%s"),
			*GetNameSafe(SpeakerPlayerState),
			CompressedVoiceData.Num(),
			*VoiceAudienceToString(Audience));
		return;
	}

	int32 RoutedTargetCount = 0;
	int32 CheckedTargetCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AShooterPlayerController* ListenerController = Cast<AShooterPlayerController>(It->Get());
		if (!IsValid(ListenerController) || ListenerController == this)
		{
			continue;
		}

		APlayerState* ListenerPlayerState = ListenerController->PlayerState;
		const bool bCanHearVoice = VoiceChatSubsystem->CanHearVoice(ListenerPlayerState, SpeakerPlayerState, Audience);
		++CheckedTargetCount;
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ServerRouteCheck] Speaker=%s Listener=%s CanHear=%s Audience=%s"),
			*GetNameSafe(SpeakerPlayerState),
			*GetNameSafe(ListenerPlayerState),
			bCanHearVoice ? TEXT("true") : TEXT("false"),
			*VoiceAudienceToString(Audience));

		if (bCanHearVoice)
		{
			++RoutedTargetCount;
			ListenerController->ClientReceiveCompressedVoicePacket(PlayerState, CompressedVoiceData, Audience);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ServerRoute] Speaker=%s CompressedBytes=%d Checked=%d Targets=%d Audience=%s"),
		*GetNameSafe(SpeakerPlayerState),
		CompressedVoiceData.Num(),
		CheckedTargetCount,
		RoutedTargetCount,
		*VoiceAudienceToString(Audience));
}

void AShooterPlayerController::ClientReceiveCompressedVoicePacket_Implementation(
	APlayerState* SpeakerPlayerState,
	const TArray<uint8>& CompressedVoiceData,
	const EVoiceChatAudience Audience)
{
	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ClientReceive] Blocked: not local. PC=%s Speaker=%s Bytes=%d Audience=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SpeakerPlayerState),
			CompressedVoiceData.Num(),
			*VoiceAudienceToString(Audience));
		return;
	}

	if (!IsValid(SpeakerPlayerState))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ClientReceive] Blocked: SpeakerPlayerState=null. PC=%s Bytes=%d Audience=%s"),
			*GetNameSafe(this),
			CompressedVoiceData.Num(),
			*VoiceAudienceToString(Audience));
		return;
	}

	if (CompressedVoiceData.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ClientReceive] Blocked: empty packet. PC=%s Speaker=%s Audience=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SpeakerPlayerState),
			*VoiceAudienceToString(Audience));
		return;
	}

	UVoiceChatSubsystem* VoiceChatSubsystem = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		VoiceChatSubsystem = GameInstance->GetSubsystem<UVoiceChatSubsystem>();
	}

	if (IsValid(VoiceChatSubsystem) && !VoiceChatSubsystem->CanHearVoice(PlayerState, SpeakerPlayerState, Audience))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ClientReceive] Blocked: CanHearVoice=false. Listener=%s Speaker=%s Bytes=%d Audience=%s"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(SpeakerPlayerState),
			CompressedVoiceData.Num(),
			*VoiceAudienceToString(Audience));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ClientReceive] Accepted. Listener=%s Speaker=%s CompressedBytes=%d Audience=%s VoiceComponent=%s"),
		*GetNameSafe(PlayerState),
		*GetNameSafe(SpeakerPlayerState),
		CompressedVoiceData.Num(),
		*VoiceAudienceToString(Audience),
		IsValid(SteamVoiceComponent) ? TEXT("valid") : TEXT("null"));

	if (SteamVoiceComponent)
	{
		SteamVoiceComponent->PlayCompressedVoicePacket(SpeakerPlayerState, CompressedVoiceData);
	}

	ReceiveCompressedVoicePacket(SpeakerPlayerState, CompressedVoiceData);
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

void AShooterPlayerController::Input_PushToTalkPressed()
{
	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Input] PushToTalkPressed PC=%s Local=%s VoiceComponent=%s"),
		*GetNameSafe(this),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		IsValid(SteamVoiceComponent) ? TEXT("valid") : TEXT("null"));
	StartVoiceTalking();
}

void AShooterPlayerController::Input_PushToTalkReleased()
{
	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Input] PushToTalkReleased PC=%s Local=%s VoiceComponent=%s"),
		*GetNameSafe(this),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		IsValid(SteamVoiceComponent) ? TEXT("valid") : TEXT("null"));
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
