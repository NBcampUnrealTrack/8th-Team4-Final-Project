#include "Player/ShooterPlayerController.h"

#include "AudioDevice.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/InputSettings.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GAS/RogueHealthComponent.h"
#include "GameMode/BaseTeamGameMode.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "GameMode/LobbyGameMode.h"
#include "GameMode/ShooterGameMode.h"
#include "Input/ShooterInputConfig.h"
#include "Input/ShooterInteractionComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Interfaces/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Killcam/KillcamReplaySubsystem.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/NetworkTimeSyncComponent.h"
#include "Player/ShooterPlayerState.h"
#include "Session/SessionSubSystem.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Minimap/ShooterMinimapSubsystem.h"
#include "Settings/ShooterAudioSettings.h"
#include "Settings/ShooterGameUserSettings.h"
#include "Tags/GameplayTags.h"
#include "UI/Loading/LoadingScreenWidget.h"
#include "UI/Frontend/TeamMatchUIInitComponent.h"
#include "UI/InGame/HUD/ShooterChatInputBox.h"
#include "UI/InGame/Spectator/ShooterSpectatorWidget.h"
#include "Voice/SteamVoiceComponent.h"
#include "Voice/VoiceChatSubsystem.h"
#include "Player/ShooterSpectatorPawn.h"
#include "Weapon/WeaponBase.h"
#include "UObject/UObjectGlobals.h"

AShooterPlayerController::AShooterPlayerController()
{
	NetworkTimeSyncComponent = CreateDefaultSubobject<UNetworkTimeSyncComponent>(TEXT("NetworkTimeSyncComponent"));
	SteamVoiceComponent = CreateDefaultSubobject<USteamVoiceComponent>(TEXT("SteamVoiceComponent"));
	InteractionComponent = CreateDefaultSubobject<UShooterInteractionComponent>(TEXT("InteractionComponent"));
}

void AShooterPlayerController::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(
		AShooterPlayerController,
		SurrenderVoteState,
		COND_OwnerOnly,
		REPNOTIFY_Always);
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();
    SubmitSavedAppearances();

	if (!IsLocalController())
	{
		return;
	}

	// FPS 마우스 입력은 보간/가속 없이 선형 입력으로 사용한다.
	if (UInputSettings* EngineInputSettings = GetMutableDefault<UInputSettings>())
	{
		EngineInputSettings->bEnableMouseSmoothing = false;
		EngineInputSettings->bEnableFOVScaling = false;
		EngineInputSettings->bEnableLegacyInputScales = false;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UKillcamReplaySubsystem* KillcamSubsystem = LocalPlayer->GetSubsystem<UKillcamReplaySubsystem>())
		{
			KillcamSubsystem->SetUpForWorld(GetWorld());
		}
	}

	if (const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		Settings->ApplyLanguageCulture();
	}

	InitializeAudioSettings();
	InitializeRuntimeInputMappings();

	if (GetTeamMatchUIInitComponent())
	{
		SetSystemMappingContextEnabled(true);
	}

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		RefreshLoadingScreenGameStateBinding(LoadingScreenSubsystem);
	}

	StartMatchLoadReadyCheck();
}

void AShooterPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(AppearanceSubmitTimer);
	if (HasAuthority())
	{
		if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
		{
			ShooterPlayerState->SetVoiceTalking(false);
		}
	}

	if (IsLocalController())
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UKillcamReplaySubsystem* KillcamSubsystem = LocalPlayer->GetSubsystem<UKillcamReplaySubsystem>())
			{
				KillcamSubsystem->ReleaseWorld(GetWorld());
			}
		}

		ShutdownAudioSettings();
		HideSpectatorWidget();

		if (GetTeamMatchUIInitComponent())
		{
			SetSystemMappingContextEnabled(false);
		}
		ShutdownRuntimeInputMappings();
		StopMatchLoadReadyCheck();

		if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
		{
			// GameInstanceSubsystem은 World보다 오래 산다. World Actor 참조는
			// PlayerController가 끝나는 경계에서 명시적으로 끊는다.
			ClearLoadingScreenGameStateBinding(LoadingScreenSubsystem);

			// seamless 교체로 LocalPlayer를 넘긴 PC는 걷지 않음
			if (GetLocalPlayer())
			{
				LoadingScreenSubsystem->HideLoadingScreen();
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AShooterPlayerController::InitializeAudioSettings()
{
	UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}

	Settings->OnAudioSettingsChanged.AddUObject(this, &ThisClass::HandleAudioSettingsChanged);

	ActiveUserSettingsSoundMix = Settings->GetUserSettingsSoundMix();
	if (ActiveUserSettingsSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(this, ActiveUserSettingsSoundMix);
	}

	HandleAudioSettingsChanged();
}

void AShooterPlayerController::ShutdownAudioSettings()
{
	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		Settings->OnAudioSettingsChanged.RemoveAll(this);
	}

	if (UWorld* World = GetWorld())
	{
		if (FAudioDevice* AudioDevice = World->GetAudioDeviceRaw())
		{
			AudioDevice->SetTransientPrimaryVolume(1.0f);
		}
	}

	if (ActiveUserSettingsSoundMix)
	{
		UGameplayStatics::PopSoundMixModifier(this, ActiveUserSettingsSoundMix);
		ActiveUserSettingsSoundMix = nullptr;
	}
}

void AShooterPlayerController::HandleAudioSettingsChanged()
{
	UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}

	if (SteamVoiceComponent)
	{
		SteamVoiceComponent->SetOutputVolume(Settings->GetVoiceChatOutputVolume());
		if (!Settings->IsVoiceChatEnabled() || !SupportsVoiceChat())
		{
			if (SteamVoiceComponent->IsCapturingVoice())
			{
				StopVoiceTalking();
			}
			SteamVoiceComponent->StopPlayback();
		}
	}

	const float MasterVolume = Settings->IsMasterAudioMuted()
		? 0.0f
		: Settings->GetAudioVolume(EShooterAudioChannel::Master);

	// Master volume must work even when project SoundClass assets are not configured yet.
	// Apply it at the world's audio-device output stage; channel sliders still use SoundMix/SoundClass.
	if (UWorld* World = GetWorld())
	{
		if (FAudioDevice* AudioDevice = World->GetAudioDeviceRaw())
		{
			AudioDevice->SetTransientPrimaryVolume(FMath::Clamp(MasterVolume, 0.0f, 2.0f));
		}
	}

	if (!ActiveUserSettingsSoundMix)
	{
		return;
	}

	const auto SetClassVolume = [this, Settings](
		const EShooterAudioChannel Channel,
		const float Volume,
		const bool bApplyToChildren)
	{
		USoundClass* SoundClass = Settings->GetAudioChannelSoundClass(Channel);
		if (!SoundClass)
		{
			return;
		}

		UGameplayStatics::SetSoundMixClassOverride(
			this,
			ActiveUserSettingsSoundMix,
			SoundClass,
			FMath::Clamp(Volume, 0.0f, 2.0f),
			1.0f,
			0.0f,
			bApplyToChildren);
	};

	// Channel overrides are multiplied by the already-applied master value.
	SetClassVolume(EShooterAudioChannel::Master, 1.0f, false);
	SetClassVolume(EShooterAudioChannel::Music,
		Settings->GetAudioVolume(EShooterAudioChannel::Music), true);
	SetClassVolume(EShooterAudioChannel::SoundEffects,
		Settings->GetAudioVolume(EShooterAudioChannel::SoundEffects), true);
	SetClassVolume(EShooterAudioChannel::UserInterface,
		Settings->GetAudioVolume(EShooterAudioChannel::UserInterface), true);
	// Voice is the game's announcer/dialogue channel. Voice chat has its own output
	// multiplier on USteamVoiceComponent, so disabling chat must not mute announcers.
	SetClassVolume(EShooterAudioChannel::Voice,
		Settings->GetAudioVolume(EShooterAudioChannel::Voice),
		true);
}

void AShooterPlayerController::NotifyLoadedWorld(FName WorldPackageName, bool bFinalDest)
{
	Super::NotifyLoadedWorld(WorldPackageName, bFinalDest);

	if (!bFinalDest || !IsLocalController())
	{
		return;
	}

	// 이 플래그는 World 단위의 1회성 상태다. 로비에서 한 번 소비한 값을
	// 다음 인게임 World까지 들고 가지 않도록 최종 목적지마다 재무장한다.
	bNotifiedLoadedIntoMatch = false;

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		RefreshLoadingScreenGameStateBinding(LoadingScreenSubsystem);
		LoadingScreenSubsystem->ShowLoadingScreen(this, LoadingScreenWidgetClass, 0.5f);
		LoadingScreenSubsystem->NotifyWorldLoaded();
	}

	StartMatchLoadReadyCheck();
}

void AShooterPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
    SubmitSavedAppearances();

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
	if (InputConfig->AdjustADSZoomAction)
	{
		EnhancedInputComponent->BindAction(
			InputConfig->AdjustADSZoomAction, ETriggerEvent::Triggered, this, &ThisClass::Input_AdjustADSZoom);
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
	if (InputConfig->CrouchAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->CrouchAction, ETriggerEvent::Started, this, &ThisClass::Input_CrouchPressed);
		EnhancedInputComponent->BindAction(InputConfig->CrouchAction, ETriggerEvent::Completed, this, &ThisClass::Input_CrouchReleased);
		EnhancedInputComponent->BindAction(InputConfig->CrouchAction, ETriggerEvent::Canceled, this, &ThisClass::Input_CrouchReleased);
	}
	if (SupportsVoiceChat() && InputConfig->PushToTalkAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->PushToTalkAction, ETriggerEvent::Started, this, &ThisClass::Input_PushToTalkPressed);
		EnhancedInputComponent->BindAction(InputConfig->PushToTalkAction, ETriggerEvent::Completed, this, &ThisClass::Input_PushToTalkReleased);
		EnhancedInputComponent->BindAction(InputConfig->PushToTalkAction, ETriggerEvent::Canceled, this, &ThisClass::Input_PushToTalkReleased);
	}
	if (InputConfig->OpenChatAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->OpenChatAction, ETriggerEvent::Started, this, &ThisClass::Input_OpenChat);
	}
	if (InputConfig->PingAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->PingAction, ETriggerEvent::Started, this, &ThisClass::Input_Ping);
	}
	if (InputConfig->ToggleFullMapAction)
	{
		EnhancedInputComponent->BindAction(
			InputConfig->ToggleFullMapAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleFullMap);
	}
	if (InputConfig->ToggleInGameMenuAction)
	{
		EnhancedInputComponent->BindAction(
			InputConfig->ToggleInGameMenuAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::Input_ToggleInGameMenu);
	}
	const UTeamMatchUIInitComponent* ScoreboardUI = GetTeamMatchUIInitComponent();
	if (InputConfig->ShowScoreboardAction && ScoreboardUI && ScoreboardUI->SupportsScoreboard())
	{
		EnhancedInputComponent->BindAction(
			InputConfig->ShowScoreboardAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::Input_ShowScoreboard);

		EnhancedInputComponent->BindAction(
			InputConfig->ShowScoreboardAction,
			ETriggerEvent::Completed,
			this,
			&ThisClass::Input_HideScoreboard);

		EnhancedInputComponent->BindAction(
			InputConfig->ShowScoreboardAction,
			ETriggerEvent::Canceled,
			this,
			&ThisClass::Input_HideScoreboard);
	}

	if (InputConfig->InteractAction)
	{
		EnhancedInputComponent->BindAction(InputConfig->InteractAction, ETriggerEvent::Started,
			this, &ThisClass::Input_InteractPressed);
		EnhancedInputComponent->BindAction(InputConfig->InteractAction, ETriggerEvent::Completed,
			this, &ThisClass::Input_InteractReleased);
		EnhancedInputComponent->BindAction(InputConfig->InteractAction, ETriggerEvent::Canceled,
			this, &ThisClass::Input_InteractReleased);
	}

	for (const FShooterAbilityInputAction& AbilityInput : InputConfig->AbilityInputActions)
	{
		if (!IsValid(AbilityInput.InputAction) || !AbilityInput.InputTag.IsValid()
			|| AbilityInput.InputAction == InputConfig->InteractAction)
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

void AShooterPlayerController::Input_InteractPressed(const FInputActionValue& Value)
{
	if (!CanProcessActionInput())
	{
		return;
	}
	ActiveInteractionFallbackTag = FGameplayTag();
	if (InteractionComponent->TryInteract())
	{
		return;
	}

	for (const FShooterAbilityInputAction& AbilityInput : InputConfig->AbilityInputActions)
	{
		if (AbilityInput.InputAction == InputConfig->InteractAction && AbilityInput.InputTag.IsValid())
		{
			ActiveInteractionFallbackTag = AbilityInput.InputTag;
			Input_AbilityPressed(Value, ActiveInteractionFallbackTag);
			break;
		}
	}
}

void AShooterPlayerController::Input_InteractReleased(const FInputActionValue& Value)
{
	if (ActiveInteractionFallbackTag.IsValid())
	{
		Input_AbilityReleased(Value, ActiveInteractionFallbackTag);
		ActiveInteractionFallbackTag = FGameplayTag();
	}
}

void AShooterPlayerController::RefreshLoadingScreenGameStateBinding(
	ULoadingScreenSubsystem*)
{
}

void AShooterPlayerController::ClearLoadingScreenGameStateBinding(
	ULoadingScreenSubsystem* LoadingScreenSubsystem)
{
	// 해제는 바인딩된 GameState 타입을 몰라도 되므로 공용 계층에서 처리한다.
	if (IsValid(LoadingScreenSubsystem))
	{
		LoadingScreenSubsystem->UnbindFromRoundGameState();
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
	if (!IsLocalController() || !SupportsVoiceChat())
	{
		return;
	}

	if (const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
		!Settings || !Settings->IsVoiceChatEnabled())
	{
		return;
	}

	if (!SteamVoiceComponent || !SteamVoiceComponent->StartCapture())
	{
		return;
	}

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
	if (IsLocalController() && SteamVoiceComponent)
	{
		SteamVoiceComponent->StopCapture();
	}

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

void AShooterPlayerController::SubmitCompressedVoicePacket(const TArray<uint8>& CompressedVoiceData)
{
	if (!IsLocalController() || !SupportsVoiceChat() || CompressedVoiceData.IsEmpty())
	{
		return;
	}

	const AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>();
	if (!IsValid(ShooterPlayerState) || !ShooterPlayerState->IsVoiceTalking())
	{
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

	const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	const float MicrophoneVolume = Settings
		? FMath::Clamp(Settings->GetVoiceChatMicrophoneVolume(), 0.0f, 2.0f)
		: 1.0f;
	if (MicrophoneVolume <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (HasAuthority())
	{
		ServerSubmitCompressedVoicePacket_Implementation(CompressedVoiceData, Audience, MicrophoneVolume);
		return;
	}

	ServerSubmitCompressedVoicePacket(CompressedVoiceData, Audience, MicrophoneVolume);
}

void AShooterPlayerController::ToggleDynamicCrosshair()
{
	UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}

	Settings->SetDynamicCrosshairEnabled(!Settings->IsDynamicCrosshairEnabled());
	Settings->SaveSettings();
}

void AShooterPlayerController::TestHitIndicator()
{
	AShooterPlayerState* ShooterPS = GetPlayerState<AShooterPlayerState>();
	URogueHealthComponent* HealthComponent = ShooterPS ? ShooterPS->GetHealthComponent() : nullptr;
	if (!HealthComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TestHitIndicator] PlayerState=%s HealthComponent=%s - 둘 중 하나가 없어서 중단."),
			*GetNameSafe(ShooterPS), *GetNameSafe(HealthComponent));
		return;
	}

	// 데미지 파이프라인을 거치지 않고, 실제로 오른쪽에서 맞은 것처럼 방향만 흉내내서
	// 인디케이터 UI(회전/페이드)가 직접 잘 동작하는지만 검증한다.
	const APawn* ControlledPawn = GetPawn();
	const FVector TestDirection = IsValid(ControlledPawn) ? ControlledPawn->GetActorRightVector() : FVector::RightVector;

	UE_LOG(LogTemp, Warning, TEXT("[TestHitIndicator] 브로드캐스트 시도. 구독자 있음=%s"),
		HealthComponent->OnHitReactionReceived.IsBound() ? TEXT("true") : TEXT("false"));

	HealthComponent->OnHitReactionReceived.Broadcast(TestDirection);
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

void AShooterPlayerController::RequestStartRoomGame()
{
	if (HasAuthority())
	{
		ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr;
		if (!IsValid(LobbyGameMode) || !LobbyGameMode->RequestStartRoomGame(this))
		{
			ClientNotifyLobbyStartFailed();
		}
		return;
	}

	ServerRequestStartRoomGame();
}

void AShooterPlayerController::RequestLobbyMatchMode(const ELobbyMatchMode RequestedMatchMode)
{
	if (HasAuthority())
	{
		if (ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr)
		{
			LobbyGameMode->RequestLobbyMatchMode(this, RequestedMatchMode);
		}
		return;
	}

	ServerRequestLobbyMatchMode(RequestedMatchMode);
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
		RefreshLoadingScreenGameStateBinding(LoadingScreenSubsystem);
		LoadingScreenSubsystem->ShowLoadingScreen(this, LoadingScreenWidgetClass, InitialProgress);
	}

	if (UWorld* World = GetWorld())
	{
		if (World->GetGameState<ABaseTeamGameState>())
		{
			StartMatchLoadReadyCheck();
		}
	}
}

void AShooterPlayerController::SetLoadingProgress(const float Progress)
{
	if (!IsLocalController())
	{
		return;
	}

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		RefreshLoadingScreenGameStateBinding(LoadingScreenSubsystem);
		if (Progress < 1.0f && !LoadingScreenSubsystem->IsLoadingScreenVisible())
		{
			LoadingScreenSubsystem->ShowLoadingScreen(this, LoadingScreenWidgetClass, Progress);
			return;
		}

		LoadingScreenSubsystem->SetLoadingProgress(Progress);
	}
}

void AShooterPlayerController::HideLoadingScreen()
{
	if (!IsLocalController())
	{
		return;
	}

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingScreenSubsystem->HideLoadingScreen();
	}
}

void AShooterPlayerController::FinishLoadingScreen()
{
	if (!IsLocalController())
	{
		return;
	}

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingScreenSubsystem->FinishLoadingScreen();
	}
}

void AShooterPlayerController::RequestReturnToLobby()
{
	if (!IsLocalController())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	USessionSubSystem* SessionSubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<USessionSubSystem>()
		: nullptr;

	// 공유 로비 맵은 서버만 이동시킬 수 있다. 리슨 호스트는 방을 유지한 채
	// 전원을 로비로 돌리고, 일반 클라이언트는 현재 방을 정상적으로 떠난다.
	if (HasAuthority() && IsValid(SessionSubsystem))
	{
		if (SessionSubsystem->ReturnRoomToLobby())
		{
			return;
		}
	}

	RequestReturnToFrontend();
}

void AShooterPlayerController::RequestReturnToFrontend()
{
	if (!IsLocalController())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return;
	}

	if (USessionSubSystem* SessionSubsystem = GameInstance->GetSubsystem<USessionSubSystem>())
	{
		SessionSubsystem->LeaveRoomAndReturnToFrontend();
		return;
	}

	GameInstance->ReturnToMainMenu();
}

void AShooterPlayerController::RequestQuitGame()
{
	if (IsLocalController())
	{
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
}

const FSurrenderVoteState& AShooterPlayerController::GetSurrenderVoteState() const
{
	return SurrenderVoteState;
}

void AShooterPlayerController::RequestStartSurrenderVote()
{
	if (IsLocalController())
	{
		ServerRequestStartSurrenderVote();
	}
}

void AShooterPlayerController::SubmitSurrenderVote(const bool bApprove)
{
	if (IsLocalController() && SurrenderVoteState.IsActive())
	{
		ServerSubmitSurrenderVote(SurrenderVoteState.VoteId, bApprove);
	}
}

void AShooterPlayerController::ServerRequestStartSurrenderVote_Implementation()
{
	ABaseTeamGameMode* TeamGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ABaseTeamGameMode>()
		: nullptr;
	const ESurrenderVoteStartResult Result = IsValid(TeamGameMode)
		? TeamGameMode->RequestStartSurrenderVote(this)
		: ESurrenderVoteStartResult::NotAllowed;

	if (IsLocalController())
	{
		OnSurrenderVoteStartResult.Broadcast(Result);
	}
	else
	{
		ClientNotifySurrenderVoteStartResult(Result);
	}
}

void AShooterPlayerController::ServerSubmitSurrenderVote_Implementation(
	const int32 VoteId,
	const bool bApprove)
{
	if (ABaseTeamGameMode* TeamGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ABaseTeamGameMode>()
		: nullptr)
	{
		TeamGameMode->SubmitSurrenderVote(this, VoteId, bApprove);
	}
}

void AShooterPlayerController::ClientNotifySurrenderVoteStartResult_Implementation(
	const ESurrenderVoteStartResult Result)
{
	OnSurrenderVoteStartResult.Broadcast(Result);
}

void AShooterPlayerController::OnRep_SurrenderVoteState()
{
	OnSurrenderVoteStateChanged.Broadcast();
}

void AShooterPlayerController::SetSurrenderVoteState(
	const FSurrenderVoteState& NewState)
{
	SurrenderVoteState = NewState;
	ForceNetUpdate();

	if (IsLocalController())
	{
		OnRep_SurrenderVoteState();
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

void AShooterPlayerController::ClientFinishLoadingScreen_Implementation()
{
	FinishLoadingScreen();
}

void AShooterPlayerController::ClientPlayHitMarker_Implementation(const ERogueHitMarkerType Type, const bool bKill, const bool bPlaySound)
{
	OnHitMarker.Broadcast(Type, bKill);

	// 사운드는 PlayerController 하위 클래스마다 따로 두지 않고 UShooterAudioSettings 하나로 통합돼 있다.
	const UShooterAudioSettings* AudioSettings = GetDefault<UShooterAudioSettings>();

	// 헤드샷 처치는 전용 사운드를 최우선으로, 그 외 처치는 일반 처치 사운드를 우선 재생하고,
	// 둘 다 없으면 부위별 사운드로 대체한다.
	USoundBase* Sound = nullptr;
	if (bKill && Type == ERogueHitMarkerType::Headshot)
	{
		Sound = AudioSettings->HitMarkerSound_HeadshotKill.LoadSynchronous();
	}
	if (!Sound && bKill)
	{
		Sound = AudioSettings->HitMarkerSound_Kill.LoadSynchronous();
	}
	if (!Sound)
	{
		switch (Type)
		{
		case ERogueHitMarkerType::Headshot:
			Sound = AudioSettings->HitMarkerSound_Headshot.LoadSynchronous();
			break;
		case ERogueHitMarkerType::Shield:
			// 실드 히트 사운드는 뺀다(기획 요청).
			break;
		case ERogueHitMarkerType::Body:
		default:
			Sound = AudioSettings->HitMarkerSound_Body.LoadSynchronous();
			break;
		}
	}

	if (bPlaySound && Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

void AShooterPlayerController::ClientPlayKillSound_Implementation(const int32 KillStreak)
{
	const UShooterAudioSettings* AudioSettings = GetDefault<UShooterAudioSettings>();

	// 스트릭별 사운드가 지정돼 있으면 그걸 우선 재생하고, 없으면 기본 킬 사운드로 대체한다.
	USoundBase* Sound = nullptr;
	switch (KillStreak)
	{
	case 2:
		Sound = AudioSettings->DoubleKillSound.LoadSynchronous();
		break;
	case 3:
		Sound = AudioSettings->TripleKillSound.LoadSynchronous();
		break;
	default:
		if (KillStreak >= 4)
		{
			Sound = AudioSettings->QuadraKillSound.LoadSynchronous();
		}
		break;
	}

	if (!Sound)
	{
		Sound = AudioSettings->LocalKillSound.LoadSynchronous();
	}

	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}

	OnKillEffect.Broadcast();
}

void AShooterPlayerController::ClientEnterSpectatorMode_Implementation(APlayerState* ViewTargetPlayerState)
{
	ShowSpectatorWidget(ViewTargetPlayerState);
}

void AShooterPlayerController::ClientSetSpectatorTarget_Implementation(APlayerState* ViewTargetPlayerState)
{
	RefreshSpectatorWidget(ViewTargetPlayerState);
}

void AShooterPlayerController::ClientExitSpectatorMode_Implementation()
{
	HideSpectatorWidget();
}

void AShooterPlayerController::SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams)
{
	if (bKillcamViewTargetSuppressed)
	{
		const bool bUsableCandidate = NewViewTarget != this
			&& UKillcamReplaySubsystem::IsLiveWorldActor(NewViewTarget);
		if (bUsableCandidate)
		{
			PendingViewTarget = NewViewTarget;
			PendingViewTargetTransitionParams = TransitionParams;
		}
		return;
	}

	Super::SetViewTarget(NewViewTarget, TransitionParams);
}

void AShooterPlayerController::UpdateHiddenComponents(
	const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents)
{
	Super::UpdateHiddenComponents(ViewLocation, HiddenComponents);
	if (!bKillcamViewTargetSuppressed || !IsLocalController() || !GetWorld())
	{
		return;
	}

	const FLevelCollection* LiveCollection = GetWorld()->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	if (!LiveCollection)
	{
		return;
	}

	TSet<AActor*> LiveActors;
	for (ULevel* Level : LiveCollection->GetLevels())
	{
		if (!IsValid(Level))
		{
			continue;
		}

		for (AActor* Actor : Level->Actors)
		{
			if (!IsValid(Actor) || (!Actor->IsA<APawn>() && !Actor->IsA<AWeaponBase>()))
			{
				continue;
			}

			LiveActors.Add(Actor);
			TArray<AActor*> AttachedActors;
			Actor->GetAttachedActors(AttachedActors, true, true);
			for (AActor* AttachedActor : AttachedActors)
			{
				if (IsValid(AttachedActor) && UKillcamReplaySubsystem::IsLiveWorldActor(AttachedActor))
				{
					LiveActors.Add(AttachedActor);
				}
			}
		}
	}

	for (AActor* Actor : LiveActors)
	{
		TInlineComponentArray<UPrimitiveComponent*> Components;
		Actor->GetComponents(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			if (IsValid(Component) && Component->IsRegistered())
			{
				HiddenComponents.Add(Component->GetPrimitiveSceneId());
			}
		}
	}
}

void AShooterPlayerController::SetKillcamViewTargetSuppressed(const bool bSuppressed)
{
	if (bKillcamViewTargetSuppressed == bSuppressed)
	{
		return;
	}

	bKillcamViewTargetSuppressed = bSuppressed;
	if (bSuppressed)
	{
		PendingViewTarget.Reset();
		PendingViewTargetTransitionParams = FViewTargetTransitionParams();
	}
}

void AShooterPlayerController::ApplyKillcamViewTarget(AActor* KillcamCamera)
{
	if (!IsValid(KillcamCamera))
	{
		return;
	}

	Super::SetViewTarget(KillcamCamera, FViewTargetTransitionParams());
}

AActor* AShooterPlayerController::ConsumePendingViewTarget()
{
	AActor* Pending = PendingViewTarget.Get();
	PendingViewTarget.Reset();
	PendingViewTargetTransitionParams = FViewTargetTransitionParams();
	return Pending;
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
		ShooterPlayerState->SetVoiceTalking(SupportsVoiceChat() && bNewVoiceTalking);
	}
}

void AShooterPlayerController::ServerSubmitCompressedVoicePacket_Implementation(
	const TArray<uint8>& CompressedVoiceData,
	const EVoiceChatAudience Audience,
	const float MicrophoneVolume)
{
	if (!SupportsVoiceChat() || CompressedVoiceData.IsEmpty())
	{
		return;
	}

	const APlayerState* SpeakerPlayerState = PlayerState;
	if (!IsValid(SpeakerPlayerState))
	{
		return;
	}

	UVoiceChatSubsystem* VoiceChatSubsystem = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		VoiceChatSubsystem = GameInstance->GetSubsystem<UVoiceChatSubsystem>();
	}

	if (!IsValid(VoiceChatSubsystem) || !VoiceChatSubsystem->CanSendVoice(SpeakerPlayerState, Audience))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	int32 RoutedTargetCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AShooterPlayerController* ListenerController = Cast<AShooterPlayerController>(It->Get());
		if (!IsValid(ListenerController) || ListenerController == this)
		{
			continue;
		}

		APlayerState* ListenerPlayerState = ListenerController->PlayerState;
		if (VoiceChatSubsystem->CanHearVoice(ListenerPlayerState, SpeakerPlayerState, Audience))
		{
			++RoutedTargetCount;
			ListenerController->ClientReceiveCompressedVoicePacket(
				PlayerState,
				CompressedVoiceData,
				Audience,
				FMath::Clamp(MicrophoneVolume, 0.0f, 2.0f));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ServerRoute] Speaker=%s CompressedBytes=%d Targets=%d"),
		*GetNameSafe(SpeakerPlayerState),
		CompressedVoiceData.Num(),
		RoutedTargetCount);
}

void AShooterPlayerController::ClientReceiveCompressedVoicePacket_Implementation(
	APlayerState* SpeakerPlayerState,
	const TArray<uint8>& CompressedVoiceData,
	const EVoiceChatAudience Audience,
	const float MicrophoneVolume)
{
	if (!IsLocalController() || !SupportsVoiceChat() || !IsValid(SpeakerPlayerState) || CompressedVoiceData.IsEmpty())
	{
		return;
	}

	if (const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
		!Settings || !Settings->IsVoiceChatEnabled())
	{
		return;
	}

	UVoiceChatSubsystem* VoiceChatSubsystem = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		VoiceChatSubsystem = GameInstance->GetSubsystem<UVoiceChatSubsystem>();
	}

	if (IsValid(VoiceChatSubsystem) && !VoiceChatSubsystem->CanHearVoice(PlayerState, SpeakerPlayerState, Audience))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][ClientReceive] Listener=%s Speaker=%s CompressedBytes=%d"),
		*GetNameSafe(PlayerState),
		*GetNameSafe(SpeakerPlayerState),
		CompressedVoiceData.Num());

	if (SteamVoiceComponent)
	{
		SteamVoiceComponent->PlayCompressedVoicePacket(
			SpeakerPlayerState,
			CompressedVoiceData,
			FMath::Clamp(MicrophoneVolume, 0.0f, 2.0f));
	}

	ReceiveCompressedVoicePacket(SpeakerPlayerState, CompressedVoiceData);
}

void AShooterPlayerController::ServerSendChatMessage_Implementation(const FString& MessageText, const EChatChannel Channel)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (LastChatMessageServerTime >= 0.0 && Now - LastChatMessageServerTime < ShooterChatLimits::MinSendIntervalSeconds)
	{
		return;
	}

	FString Trimmed = MessageText;
	Trimmed.TrimStartAndEndInline();
	if (Trimmed.IsEmpty())
	{
		return;
	}
	if (Trimmed.Len() > ShooterChatLimits::MaxMessageLength)
	{
		Trimmed.LeftInline(ShooterChatLimits::MaxMessageLength);
	}

	LastChatMessageServerTime = Now;

	if (!IsValid(PlayerState))
	{
		return;
	}

	ABaseTeamGameState* TeamGameState = GetWorld() ? GetWorld()->GetGameState<ABaseTeamGameState>() : nullptr;
	if (!IsValid(TeamGameState))
	{
		return;
	}

	FChatMessage Message;
	Message.SenderName = PlayerState->GetPlayerName();
	Message.MessageText = Trimmed;
	Message.Channel = Channel;

	TeamGameState->ForEachChatRecipient(PlayerState, Channel, [&Message](AShooterPlayerController* Recipient)
	{
		Recipient->ClientReceiveChatMessage(Message);
	});
}

void AShooterPlayerController::ServerSendWhisperMessage_Implementation(const FString& TargetName, const FString& MessageText)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (LastChatMessageServerTime >= 0.0 && Now - LastChatMessageServerTime < ShooterChatLimits::MinSendIntervalSeconds)
	{
		return;
	}

	FString Trimmed = MessageText;
	Trimmed.TrimStartAndEndInline();
	if (Trimmed.IsEmpty())
	{
		return;
	}
	if (Trimmed.Len() > ShooterChatLimits::MaxMessageLength)
	{
		Trimmed.LeftInline(ShooterChatLimits::MaxMessageLength);
	}

	LastChatMessageServerTime = Now;

	if (!IsValid(PlayerState))
	{
		return;
	}

	ABaseTeamGameState* TeamGameState = GetWorld() ? GetWorld()->GetGameState<ABaseTeamGameState>() : nullptr;
	if (!IsValid(TeamGameState))
	{
		return;
	}

	AShooterPlayerController* TargetController = TeamGameState->FindChatRecipientByName(TargetName);
	if (!TargetController)
	{
		// MVP: 닉네임을 못 찾아도 별도 에러 메시지는 없다. 채팅창은 이미 닫힌 뒤라 조용히 무시.
		return;
	}

	FChatMessage ToReceiver;
	ToReceiver.SenderName = PlayerState->GetPlayerName();
	ToReceiver.MessageText = Trimmed;
	ToReceiver.Channel = EChatChannel::Whisper;
	ToReceiver.bIsOutgoingEcho = false;
	TargetController->ClientReceiveChatMessage(ToReceiver);

	// 보낸 사람 본인에게는 "-> 대상" 형태로 에코해 무엇을 누구에게 보냈는지 확인시켜준다.
	FChatMessage Echo;
	Echo.SenderName = FString::Printf(TEXT("-> %s"), *TargetName);
	Echo.MessageText = Trimmed;
	Echo.Channel = EChatChannel::Whisper;
	Echo.bIsOutgoingEcho = true;
	ClientReceiveChatMessage(Echo);
}

void AShooterPlayerController::ClientReceiveChatMessage_Implementation(const FChatMessage& Message)
{
	// 남이 나한테 보낸 진짜 귓속말(내 에코가 아님)이면, 오버워치처럼 자동으로 그 사람과의
	// 채널로 전환하고 최근 귓말 큐(Tab 토글용)에도 등록한다.
	if (Message.Channel == EChatChannel::Whisper && !Message.bIsOutgoingEcho)
	{
		RegisterWhisperTarget(Message.SenderName);

		if (ChatInputWidget.IsValid())
		{
			ChatInputWidget->SetChannel(CurrentChatChannel, CurrentWhisperTarget);
		}
	}

	OnChatMessageReceived.Broadcast(Message);
}

void AShooterPlayerController::ServerSendPing_Implementation(const FVector_NetQuantize Location, const EShooterPingType Type)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (LastPingServerTime >= 0.0 && Now - LastPingServerTime < ShooterPingLimits::MinSendIntervalSeconds)
	{
		return;
	}
	LastPingServerTime = Now;

	if (!IsValid(PlayerState))
	{
		return;
	}

	ABaseTeamGameState* TeamGameState = GetWorld() ? GetWorld()->GetGameState<ABaseTeamGameState>() : nullptr;
	if (!IsValid(TeamGameState))
	{
		return;
	}

	FPingEvent Ping;
	Ping.Location = Location;
	Ping.Type = Type;

	// 핑도 팀 전용이라 채팅의 Team 채널 라우팅(같은 팀 전원, 본인 포함)을 그대로 재사용한다.
	TeamGameState->ForEachChatRecipient(PlayerState, EChatChannel::Team, [&Ping](AShooterPlayerController* Recipient)
	{
		Recipient->ClientReceivePing(Ping);
	});
}

void AShooterPlayerController::ClientReceivePing_Implementation(const FPingEvent& Ping)
{
	const UShooterAudioSettings* AudioSettings = GetDefault<UShooterAudioSettings>();

	USoundBase* Sound = Ping.Type == EShooterPingType::Enemy
		? AudioSettings->PingSound_Enemy.LoadSynchronous()
		: nullptr;
	if (!Sound)
	{
		Sound = AudioSettings->PingSound_Location.LoadSynchronous();
	}

	if (Sound)
	{
		// 위치 핑 특성상 방향을 듣고 찾아갈 수 있어야 하므로 2D가 아니라 핑 찍힌 지점에서
		// 나는 3D 사운드로 재생한다. 감쇠(Attenuation)는 사운드 에셋 쪽 설정을 따른다.
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Ping.Location);
	}

	OnPingReceived.Broadcast(Ping);
}

void AShooterPlayerController::RegisterWhisperTarget(const FString& PlayerName)
{
	RecentWhisperTargets.RemoveAll([&PlayerName](const FString& Existing)
	{
		return Existing.Equals(PlayerName, ESearchCase::IgnoreCase);
	});
	RecentWhisperTargets.Insert(PlayerName, 0);
	if (RecentWhisperTargets.Num() > 2)
	{
		RecentWhisperTargets.SetNum(2);
	}

	CurrentChatChannel = EChatChannel::Whisper;
	CurrentWhisperTarget = PlayerName;
}

void AShooterPlayerController::ServerNotifyLoadedIntoMatch_Implementation()
{
	AShooterGameMode* ShooterGameMode = nullptr;
	if (UWorld* World = GetWorld())
	{
		ShooterGameMode = World->GetAuthGameMode<AShooterGameMode>();
	}

	// 로비에서 늦게 도착한 RPC가 다음 매치용 로드 상태를 미리 소비하면 안 된다.
	if (!IsValid(ShooterGameMode))
	{
		return;
	}

	if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
	{
		ShooterPlayerState->SetLoadedIntoMatch(true);
	}

	ShooterGameMode->NotifyPlayerReadyForGameFlow(this);
}

void AShooterPlayerController::ServerCycleSpectatorTarget_Implementation()
{
	AShooterGameMode* ShooterGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<AShooterGameMode>()
		: nullptr;
	if (IsValid(ShooterGameMode))
	{
		ShooterGameMode->CycleSpectatorTarget(this);
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

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		StopMatchLoadReadyCheck();
		return;
	}

	ABaseTeamGameState* TeamGameState = World->GetGameState<ABaseTeamGameState>();
	if (!IsValid(TeamGameState))
	{
		// GameState가 아직 복제되지 않았다면 조금 더 기다린다. 다른 모드의
		// GameState가 이미 존재하면 로비/프론트엔드이므로 이 체크를 종료한다.
		if (IsValid(World->GetGameState()))
		{
			StopMatchLoadReadyCheck();
		}
		return;
	}

	if (!GetPlayerState<AShooterPlayerState>())
	{
		return;
	}

	SetLoadingProgress(0.9f);

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingScreenSubsystem->NotifyMatchLoadReady();
	}

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

void AShooterPlayerController::ServerRequestStartRoomGame_Implementation()
{
	RequestStartRoomGame();
}

void AShooterPlayerController::ServerRequestLobbyMatchMode_Implementation(const ELobbyMatchMode RequestedMatchMode)
{
	RequestLobbyMatchMode(RequestedMatchMode);
}

void AShooterPlayerController::ClientNotifyLobbyStartFailed_Implementation()
{
	OnLobbyStartFailed.Broadcast();
}

bool AShooterPlayerController::CanProcessGameplayInput() const
{
	const URogueAbilitySystemComponent* AbilitySystem = GetRogueAbilitySystemComponent();
	return !IsValid(AbilitySystem)
		|| !AbilitySystem->HasMatchingGameplayTag(RogueTags::State_Gameplay_InputBlocked);
}

void AShooterPlayerController::Input_ToggleInGameMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	// 전체 맵이 열려 있으면 ESC는 맵 닫기로만 쓴다.
	if (bFullMapOpen)
	{
		SetFullMapOpen(false);
		return;
	}

	if (UTeamMatchUIInitComponent* UIInitComponent = GetTeamMatchUIInitComponent())
	{
		UIInitComponent->ToggleInGameMenu();
	}
}

void AShooterPlayerController::Input_ShowScoreboard()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UTeamMatchUIInitComponent* UIInitComponent = GetTeamMatchUIInitComponent())
	{
		UIInitComponent->ShowScoreboard();
	}
}

void AShooterPlayerController::Input_HideScoreboard()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UTeamMatchUIInitComponent* UIInitComponent = GetTeamMatchUIInitComponent())
	{
		UIInitComponent->HideScoreboard();
	}
}

FName AShooterPlayerController::MakeInputBindingId(
	const FName ContextId,
	const FEnhancedActionKeyMapping& Mapping,
	const int32 MappingIndex)
{
	const FString ActionName = IsValid(Mapping.Action) ? Mapping.Action->GetName() : TEXT("None");
	return FName(*FString::Printf(
		TEXT("%s|%s|%s|%d"),
		*ContextId.ToString(),
		*ActionName,
		*Mapping.Key.GetFName().ToString(),
		MappingIndex));
}

bool AShooterPlayerController::IsRebindableKey(const FKey& Key)
{
	if (!Key.IsValid() || Key.IsGamepadKey())
	{
		return false;
	}

	return Key != EKeys::MouseX
		&& Key != EKeys::MouseY
		&& Key != EKeys::MouseWheelAxis;
}

void AShooterPlayerController::InitializeRuntimeInputMappings()
{
	if (!IsLocalController() || !IsValid(InputConfig))
	{
		return;
	}

	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		Settings->OnInputBindingsChanged.RemoveAll(this);
		Settings->OnInputBindingsChanged.AddUObject(this, &ThisClass::HandleInputBindingsChanged);
	}

	RebuildRuntimeInputMappings();
}

void AShooterPlayerController::ShutdownRuntimeInputMappings()
{
	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		Settings->OnInputBindingsChanged.RemoveAll(this);
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (RuntimeDefaultMappingContext)
			{
				InputSubsystem->RemoveMappingContext(RuntimeDefaultMappingContext);
			}
			if (RuntimeSystemMappingContext)
			{
				InputSubsystem->RemoveMappingContext(RuntimeSystemMappingContext);
			}
		}
	}

	RuntimeDefaultMappingContext = nullptr;
	RuntimeSystemMappingContext = nullptr;
	bSystemMappingContextEnabled = false;
}

void AShooterPlayerController::ApplySavedBindingsToContext(
	UInputMappingContext* RuntimeContext,
	const UInputMappingContext* SourceContext,
	const FName ContextId) const
{
	if (!IsValid(RuntimeContext) || !IsValid(SourceContext))
	{
		return;
	}

	const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}

	// Rebuild mappings action-by-action so source modifiers/triggers are preserved while
	// the authored mapping context remains untouched.
	TSet<const UInputAction*> ClearedActions;
	const TArray<FEnhancedActionKeyMapping>& SourceMappings = SourceContext->GetMappings();
	for (const FEnhancedActionKeyMapping& Mapping : SourceMappings)
	{
		if (IsValid(Mapping.Action) && !ClearedActions.Contains(Mapping.Action))
		{
			RuntimeContext->UnmapAllKeysFromAction(Mapping.Action);
			ClearedActions.Add(Mapping.Action);
		}
	}

	for (int32 Index = 0; Index < SourceMappings.Num(); ++Index)
	{
		const FEnhancedActionKeyMapping& Mapping = SourceMappings[Index];
		if (!IsValid(Mapping.Action))
		{
			continue;
		}

		FKey EffectiveKey = Mapping.Key;
		const FName OverrideName = Settings->GetKeyBindingOverride(
			MakeInputBindingId(ContextId, Mapping, Index));
		if (!OverrideName.IsNone())
		{
			const FKey OverrideKey(OverrideName);
			if (OverrideKey.IsValid())
			{
				EffectiveKey = OverrideKey;
			}
		}

		FEnhancedActionKeyMapping& NewMapping = RuntimeContext->MapKey(Mapping.Action, EffectiveKey);
		NewMapping.Modifiers = Mapping.Modifiers;
		NewMapping.Triggers = Mapping.Triggers;
	}
}

void AShooterPlayerController::RebuildRuntimeInputMappings()
{
	if (!IsLocalController() || !IsValid(InputConfig))
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = IsValid(LocalPlayer)
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (!IsValid(InputSubsystem))
	{
		return;
	}

	if (RuntimeDefaultMappingContext)
	{
		InputSubsystem->RemoveMappingContext(RuntimeDefaultMappingContext);
	}
	if (RuntimeSystemMappingContext)
	{
		InputSubsystem->RemoveMappingContext(RuntimeSystemMappingContext);
	}

	RuntimeDefaultMappingContext = IsValid(InputConfig->DefaultMappingContext)
		? DuplicateObject<UInputMappingContext>(
			InputConfig->DefaultMappingContext,
			this,
			MakeUniqueObjectName(this, UInputMappingContext::StaticClass(), TEXT("RuntimeDefaultMappingContext")))
		: nullptr;
	RuntimeSystemMappingContext = IsValid(InputConfig->SystemMappingContext)
		? DuplicateObject<UInputMappingContext>(
			InputConfig->SystemMappingContext,
			this,
			MakeUniqueObjectName(this, UInputMappingContext::StaticClass(), TEXT("RuntimeSystemMappingContext")))
		: nullptr;

	ApplySavedBindingsToContext(RuntimeDefaultMappingContext, InputConfig->DefaultMappingContext, TEXT("Default"));
	ApplySavedBindingsToContext(RuntimeSystemMappingContext, InputConfig->SystemMappingContext, TEXT("System"));

	if (RuntimeDefaultMappingContext)
	{
		InputSubsystem->AddMappingContext(RuntimeDefaultMappingContext, InputConfig->MappingPriority);
	}
	if (bSystemMappingContextEnabled && RuntimeSystemMappingContext)
	{
		InputSubsystem->AddMappingContext(RuntimeSystemMappingContext, InputConfig->SystemMappingPriority);
	}
}

void AShooterPlayerController::HandleInputBindingsChanged()
{
	RebuildRuntimeInputMappings();
}

void AShooterPlayerController::AppendRebindableBindings(
	const UInputMappingContext* SourceContext,
	const FName ContextId,
	TArray<FShooterInputBindingEntry>& OutBindings) const
{
	if (!IsValid(SourceContext) || !IsValid(InputConfig))
	{
		return;
	}

	const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	const TArray<FEnhancedActionKeyMapping>& Mappings = SourceContext->GetMappings();
	for (int32 Index = 0; Index < Mappings.Num(); ++Index)
	{
		const FEnhancedActionKeyMapping& Mapping = Mappings[Index];
		if (!IsValid(Mapping.Action)
			|| Mapping.Action == InputConfig->LookAction
			|| Mapping.Action == InputConfig->AdjustADSZoomAction
			|| (Mapping.Action == InputConfig->PushToTalkAction && !SupportsVoiceChat())
			|| !IsRebindableKey(Mapping.Key))
		{
			continue;
		}

		FShooterInputBindingEntry& Entry = OutBindings.AddDefaulted_GetRef();
		Entry.BindingId = MakeInputBindingId(ContextId, Mapping, Index);
		Entry.ContextId = ContextId;
		Entry.ActionName = Mapping.Action->GetFName();
		Entry.DefaultKey = Mapping.Key;
		Entry.CurrentKey = Mapping.Key;
		if (Settings)
		{
			const FName OverrideName = Settings->GetKeyBindingOverride(Entry.BindingId);
			if (!OverrideName.IsNone())
			{
				const FKey OverrideKey(OverrideName);
				if (OverrideKey.IsValid())
				{
					Entry.CurrentKey = OverrideKey;
				}
			}
		}
	}
}

void AShooterPlayerController::GetRebindableInputBindings(TArray<FShooterInputBindingEntry>& OutBindings) const
{
	OutBindings.Reset();
	if (!IsValid(InputConfig))
	{
		return;
	}

	AppendRebindableBindings(InputConfig->DefaultMappingContext, TEXT("Default"), OutBindings);
	AppendRebindableBindings(InputConfig->SystemMappingContext, TEXT("System"), OutBindings);
}

bool AShooterPlayerController::RebindInput(const FName BindingId, const FKey NewKey)
{
	if (!IsLocalController() || BindingId.IsNone() || !IsRebindableKey(NewKey))
	{
		return false;
	}

	UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	if (!Settings)
	{
		return false;
	}

	TArray<FShooterInputBindingEntry> Bindings;
	GetRebindableInputBindings(Bindings);
	const FShooterInputBindingEntry* Target = Bindings.FindByPredicate(
		[BindingId](const FShooterInputBindingEntry& Entry)
		{
			return Entry.BindingId == BindingId;
		});
	if (!Target)
	{
		return false;
	}

	if (Target->CurrentKey == NewKey)
	{
		return true;
	}

	// Default/System contexts can be active at the same time. Treat all exposed bindings
	// as one key space so a remap never leaves two live actions on the same key.
	const FShooterInputBindingEntry* Conflict = Bindings.FindByPredicate(
		[Target, NewKey](const FShooterInputBindingEntry& Entry)
		{
			return Entry.BindingId != Target->BindingId
				&& Entry.CurrentKey == NewKey;
		});

	if (Conflict)
	{
		Settings->SetKeyBindingOverride(Conflict->BindingId, Target->CurrentKey.GetFName());
	}
	Settings->SetKeyBindingOverride(Target->BindingId, NewKey.GetFName());
	return true;
}

void AShooterPlayerController::ResetInputBindings()
{
	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		Settings->ResetKeyBindingOverrides();
	}
}

void AShooterPlayerController::SetSystemMappingContextEnabled(const bool bEnabled)
{
	if (!IsValid(InputConfig))
	{
		return;
	}

	bSystemMappingContextEnabled = bEnabled;
	UInputMappingContext* Context = IsValid(RuntimeSystemMappingContext)
		? RuntimeSystemMappingContext.Get()
		: InputConfig->SystemMappingContext.Get();
	SetMappingContextEnabled(
		Context,
		InputConfig->SystemMappingPriority,
		bEnabled);
}

void AShooterPlayerController::SetMappingContextEnabled(
	UInputMappingContext* MappingContext,
	const int32 Priority,
	const bool bEnabled)
{
	if (!IsLocalController() || !IsValid(MappingContext))
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer))
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!IsValid(InputSubsystem))
	{
		return;
	}

	if (bEnabled)
	{
		InputSubsystem->AddMappingContext(MappingContext, Priority);
	}
	else
	{
		InputSubsystem->RemoveMappingContext(MappingContext);
	}
}

UTeamMatchUIInitComponent* AShooterPlayerController::GetTeamMatchUIInitComponent() const
{
	return FindComponentByClass<UTeamMatchUIInitComponent>();
}

bool AShooterPlayerController::CanProcessMovementInput() const
{
	return CanProcessGameplayInput() && !HasDataJackInteractionInputBlock();
}

bool AShooterPlayerController::CanProcessActionInput() const
{
	// 채팅창이 열려있는 동안은 오버워치처럼 이동은 계속 허용하고 사격/어빌리티 입력만 막는다.
	return CanProcessGameplayInput() && !HasDataJackInteractionInputBlock() && !bIsChatInputOpen;
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
		FVector2D LookAxisVector = Value.Get<FVector2D>();

		if (const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
		{
			const UCombatComponent* CombatComponent = ShooterCharacter->GetCombatComponent();
			const bool bAiming = CombatComponent && CombatComponent->bAiming;
			LookAxisVector = Settings->ApplyLookSensitivity(LookAxisVector, bAiming);
		}

		ShooterCharacter->HandleLookInput(LookAxisVector);
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

void AShooterPlayerController::Input_CrouchPressed()
{
	if (!CanProcessMovementInput())
	{
		return;
	}

	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleCrouchPressed();
	}
}

void AShooterPlayerController::Input_CrouchReleased()
{
	if (AShooterCharacter* ShooterCharacter = GetPawn<AShooterCharacter>())
	{
		ShooterCharacter->HandleCrouchReleased();
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

void AShooterPlayerController::Input_OpenChat()
{
	if (!IsLocalController() || bIsChatInputOpen)
	{
		return;
	}

	UTeamMatchUIInitComponent* UIInitComponent = GetTeamMatchUIInitComponent();
	if (!UIInitComponent)
	{
		return;
	}

	UShooterChatInputBox* Widget = UIInitComponent->OpenChat();
	if (!Widget)
	{
		return;
	}

	bIsChatInputOpen = true;
	ChatInputWidget = Widget;
	Widget->ResetForOpen(CurrentChatChannel, CurrentWhisperTarget);
}

void AShooterPlayerController::CloseChatInput()
{
	if (!bIsChatInputOpen)
	{
		return;
	}

	bIsChatInputOpen = false;
	ChatInputWidget.Reset();

	if (UTeamMatchUIInitComponent* UIInitComponent = GetTeamMatchUIInitComponent())
	{
		UIInitComponent->CloseChat();
	}
}

void AShooterPlayerController::Input_Ping()
{
	if (!IsLocalController() || !CanProcessActionInput() || bIsChatInputOpen)
	{
		return;
	}

	APawn* MyPawn = GetPawn();
	if (!MyPawn || !PlayerCameraManager)
	{
		return;
	}

	const FVector TraceStart = PlayerCameraManager->GetCameraLocation();
	const FVector TraceEnd =
		TraceStart + PlayerCameraManager->GetCameraRotation().Vector() * ShooterPingLimits::TraceDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(Ping), false, MyPawn);

	// 순수 라인트레이스 대신 작은 반경의 스피어로 스윕한다 - 크로스헤어가 캐릭터를 정확히
	// 안 맞추고 살짝 빗나가도 적 핑으로 잡히도록(오차 허용).
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(ShooterPingLimits::EnemyPingToleranceRadius);

	FHitResult Hit;
	if (!GetWorld() || !GetWorld()->SweepSingleByChannel(
		Hit, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, SphereShape, QueryParams))
	{
		return;
	}

	EShooterPingType Type = EShooterPingType::Location;

	if (const AShooterCharacter* HitCharacter = Cast<AShooterCharacter>(Hit.GetActor()))
	{
		const ABaseTeamPlayerState* HitTeamState = HitCharacter->GetPlayerState<ABaseTeamPlayerState>();
		const ABaseTeamPlayerState* MyTeamState = GetPlayerState<ABaseTeamPlayerState>();
		if (HitTeamState && MyTeamState
			&& HitTeamState->GetTeam() != EGameTeam::None
			&& HitTeamState->GetTeam() != MyTeamState->GetTeam())
		{
			Type = EShooterPingType::Enemy;
		}
	}

	ServerSendPing(Hit.Location, Type);
}

void AShooterPlayerController::Input_ToggleFullMap()
{
	// 맵은 보기 전용 UI라 사망·관전·라운드 잠금 중에도 열 수 있다. 채팅 입력 중 M은 글자로 쳐야 하므로 제외.
	if (!IsLocalController() || bIsChatInputOpen)
	{
		return;
	}

	SetFullMapOpen(!bFullMapOpen);
}

void AShooterPlayerController::SetFullMapOpen(const bool bOpen)
{
	// 전체 맵 위젯이 없거나 맵 기준 액터가 없는 레벨(훈련장 등)에서 보이지 않는 맵이 "열림"으로 남아 ESC를 한 번 먹는 걸 막는다.
	const UShooterMinimapSubsystem* MinimapSubsystem = UShooterMinimapSubsystem::Get(this);
	const bool bNewOpen = bOpen && OnFullMapOpenChanged.IsBound() && MinimapSubsystem && MinimapSubsystem->GetMapBounds();
	if (bFullMapOpen == bNewOpen)
	{
		return;
	}

	bFullMapOpen = bNewOpen;
	OnFullMapOpenChanged.Broadcast(bFullMapOpen);
}

void AShooterPlayerController::HandleChatSubmit(const FString& RawText)
{
	FString Trimmed = RawText;
	Trimmed.TrimStartAndEndInline();

	if (!Trimmed.IsEmpty())
	{
		FString TargetName, WhisperMessage;
		if (TryParseWhisperCommand(Trimmed, TargetName, WhisperMessage))
		{
			if (WhisperMessage.Len() > ShooterChatLimits::MaxMessageLength)
			{
				WhisperMessage.LeftInline(ShooterChatLimits::MaxMessageLength);
			}

			RegisterWhisperTarget(TargetName);
			ServerSendWhisperMessage(TargetName, WhisperMessage);
		}
		else if (CurrentChatChannel == EChatChannel::Whisper && !CurrentWhisperTarget.IsEmpty())
		{
			// Tab으로 귓말 채널을 골라놓은 상태에서 "/w 이름" 없이 바로 메시지만 친 경우.
			if (Trimmed.Len() > ShooterChatLimits::MaxMessageLength)
			{
				Trimmed.LeftInline(ShooterChatLimits::MaxMessageLength);
			}

			ServerSendWhisperMessage(CurrentWhisperTarget, Trimmed);
		}
		else
		{
			if (Trimmed.Len() > ShooterChatLimits::MaxMessageLength)
			{
				Trimmed.LeftInline(ShooterChatLimits::MaxMessageLength);
			}

			ServerSendChatMessage(Trimmed, CurrentChatChannel);
		}
	}

	CloseChatInput();
}

void AShooterPlayerController::HandleChatCancel()
{
	CloseChatInput();
}

bool AShooterPlayerController::TryParseWhisperCommand(const FString& Text, FString& OutTargetName, FString& OutMessage)
{
	FString Remainder;
	if (!ShooterChatLimits::StripWhisperCommandPrefix(Text, Remainder))
	{
		return false;
	}

	Remainder.TrimStartInline();

	FString Name, Message;
	if (!Remainder.Split(TEXT(" "), &Name, &Message))
	{
		return false;
	}

	Name.TrimEndInline();
	Message.TrimStartInline();
	if (Name.IsEmpty() || Message.IsEmpty())
	{
		return false;
	}

	OutTargetName = Name;
	OutMessage = Message;
	return true;
}

void AShooterPlayerController::HandleChatChannelSwitchRequested()
{
	if (!bIsChatInputOpen)
	{
		return;
	}

	// 순환 순서: Team -> Match -> 최근 귓말 대상(최대 2명, 최신순) -> Team ...
	const int32 WhisperCount = RecentWhisperTargets.Num();
	const int32 SlotCount = 2 + WhisperCount;

	int32 CurrentIndex = 0;
	if (CurrentChatChannel == EChatChannel::Match)
	{
		CurrentIndex = 1;
	}
	else if (CurrentChatChannel == EChatChannel::Whisper)
	{
		const int32 WhisperIndex = RecentWhisperTargets.IndexOfByPredicate([this](const FString& Name)
		{
			return Name.Equals(CurrentWhisperTarget, ESearchCase::IgnoreCase);
		});
		CurrentIndex = 2 + FMath::Max(WhisperIndex, 0);
	}

	const int32 NextIndex = (CurrentIndex + 1) % SlotCount;

	if (NextIndex == 0)
	{
		CurrentChatChannel = EChatChannel::Team;
		CurrentWhisperTarget.Empty();
	}
	else if (NextIndex == 1)
	{
		CurrentChatChannel = EChatChannel::Match;
		CurrentWhisperTarget.Empty();
	}
	else
	{
		CurrentChatChannel = EChatChannel::Whisper;
		CurrentWhisperTarget = RecentWhisperTargets[NextIndex - 2];
	}

	if (ChatInputWidget.IsValid())
	{
		ChatInputWidget->SetChannel(CurrentChatChannel, CurrentWhisperTarget);
	}
}

void AShooterPlayerController::Input_AdjustADSZoom(const FInputActionValue& Value)
{
	const float ZoomInput = Value.Get<float>();
	if (FMath::IsNearlyZero(ZoomInput))
	{
		return;
	}

	// 전체 맵이 열려 있으면 휠은 무기 배율 대신 맵 확대/축소로 쓴다. 보기 전용이라 행동 입력 게이트와 무관하다.
	if (bFullMapOpen)
	{
		OnFullMapZoomInput.Broadcast(ZoomInput);
		return;
	}

	if (!CanProcessActionInput())
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn) || !ControlledPawn->Implements<UPlayerInterface>())
	{
		return;
	}

	if (AWeaponBase* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(ControlledPawn))
	{
		Weapon->AdjustADSZoom(ZoomInput);
	}
}

void AShooterPlayerController::Input_AbilityPressed(const FInputActionValue&, const FGameplayTag InputTag)
{
	if (IsSpectatorCycleInput(InputTag) && IsInSpectatorFlow())
	{
		if (HasAuthority())
		{
			ServerCycleSpectatorTarget_Implementation();
		}
		else
		{
			ServerCycleSpectatorTarget();
		}
		return;
	}

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
	if (IsSpectatorCycleInput(InputTag) && IsInSpectatorFlow())
	{
		return;
	}

	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}

	// 발사 입력을 뗀 순간을 장착 무기에 알린다 - 빈 탄창에서 "클릭음 → 떼고 다시 클릭 → 재장전"
	// 2단계 입력을 성립시키기 위한 신호(AWeaponBase::NotifyPrimaryInputReleased).
	static const FGameplayTag PrimaryInputTag =
		FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Primary"), false);
	if (PrimaryInputTag.IsValid() && InputTag.MatchesTagExact(PrimaryInputTag))
	{
		APawn* ControlledPawn = GetPawn();
		if (IsValid(ControlledPawn) && ControlledPawn->Implements<UPlayerInterface>())
		{
			if (AWeaponBase* CurrentWeapon = IPlayerInterface::Execute_GetCurrentWeapon(ControlledPawn))
			{
				CurrentWeapon->NotifyPrimaryInputReleased();
			}
		}
	}
}

bool AShooterPlayerController::IsSpectatorCycleInput(const FGameplayTag InputTag) const
{
	const FGameplayTag PrimaryInputTag = FGameplayTag::RequestGameplayTag(
		TEXT("InputTag.Ability.Primary"),
		false);
	return PrimaryInputTag.IsValid() && InputTag.MatchesTagExact(PrimaryInputTag);
}

bool AShooterPlayerController::IsInSpectatorFlow() const
{
	if (GetPawn<AShooterSpectatorPawn>())
	{
		return true;
	}

	const AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>();
	return IsValid(ShooterPlayerState)
		&& ShooterPlayerState->GetParticipationState() != EPlayerParticipationState::Active;
}

void AShooterPlayerController::ShowSpectatorWidget(APlayerState* ViewTargetPlayerState)
{
	if (!IsLocalController() || !SpectatorWidgetClass)
	{
		return;
	}

	if (!SpectatorWidget)
	{
		SpectatorWidget = CreateWidget<UShooterSpectatorWidget>(this, SpectatorWidgetClass);
		if (SpectatorWidget)
		{
			SpectatorWidget->AddToViewport(100);
		}
	}

	RefreshSpectatorWidget(ViewTargetPlayerState);
}

void AShooterPlayerController::RefreshSpectatorWidget(APlayerState* ViewTargetPlayerState)
{
	if (!IsLocalController() || !SpectatorWidget)
	{
		return;
	}

	if (IsValid(ViewTargetPlayerState))
	{
		SpectatorWidget->SetSpectatorTarget(ViewTargetPlayerState);
	}
	else
	{
		SpectatorWidget->ClearSpectatorTarget();
	}
}

void AShooterPlayerController::HideSpectatorWidget()
{
	if (SpectatorWidget)
	{
		SpectatorWidget->RemoveFromParent();
		SpectatorWidget = nullptr;
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
