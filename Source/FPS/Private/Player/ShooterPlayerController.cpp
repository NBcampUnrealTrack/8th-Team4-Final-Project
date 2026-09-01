#include "Player/ShooterPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Character/ShooterCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GAS/RogueHealthComponent.h"
#include "GameMode/BaseTeamGameMode.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "GameMode/LobbyGameMode.h"
#include "GameMode/ShooterGameMode.h"
#include "Input/ShooterInputConfig.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Player/NetworkTimeSyncComponent.h"
#include "Player/ShooterPlayerState.h"
#include "Session/SessionSubSystem.h"
#include "Settings/ShooterGameUserSettings.h"
#include "Tags/GameplayTags.h"
#include "UI/Loading/LoadingScreenWidget.h"
#include "UI/Frontend/TeamMatchUIInitComponent.h"
#include "UI/InGame/Spectator/ShooterSpectatorWidget.h"
#include "Voice/SteamVoiceComponent.h"
#include "Voice/VoiceChatSubsystem.h"
#include "Player/ShooterSpectatorPawn.h"

AShooterPlayerController::AShooterPlayerController()
{
	NetworkTimeSyncComponent = CreateDefaultSubobject<UNetworkTimeSyncComponent>(TEXT("NetworkTimeSyncComponent"));
	SteamVoiceComponent = CreateDefaultSubobject<USteamVoiceComponent>(TEXT("SteamVoiceComponent"));
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

	if (!IsLocalController())
	{
		return;
	}

	if (GetTeamMatchUIInitComponent())
	{
		SetSystemMappingContextEnabled(true);
	}

	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		RefreshLoadingScreenGameStateBinding(LoadingScreenSubsystem);
	}

	StartMatchLoadReadyCheck();

	if (!InputConfig || !InputConfig->DefaultMappingContext)
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

	if (IsLocalController())
	{
		HideSpectatorWidget();

		if (GetTeamMatchUIInitComponent())
		{
			SetSystemMappingContextEnabled(false);
		}
		StopMatchLoadReadyCheck();

		if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
		{
			// GameInstanceSubsystem은 World보다 오래 산다. World Actor 참조는
			// PlayerController가 끝나는 경계에서 명시적으로 끊는다.
			ClearLoadingScreenGameStateBinding(LoadingScreenSubsystem);
			LoadingScreenSubsystem->HideLoadingScreen();
		}
	}

	Super::EndPlay(EndPlayReason);
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
	if (InputConfig->ToggleInGameMenuAction)
	{
		EnhancedInputComponent->BindAction(
			InputConfig->ToggleInGameMenuAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::Input_ToggleInGameMenu);
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
	if (IsLocalController() && SteamVoiceComponent)
	{
		SteamVoiceComponent->StartCapture();
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
	if (!IsLocalController() || CompressedVoiceData.IsEmpty())
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

void AShooterPlayerController::ClientPlayHitMarker_Implementation(const bool bRed)
{
	OnHitMarker.Broadcast(bRed);
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

void AShooterPlayerController::ServerSubmitCompressedVoicePacket_Implementation(
	const TArray<uint8>& CompressedVoiceData,
	const EVoiceChatAudience Audience)
{
	if (CompressedVoiceData.IsEmpty())
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
			ListenerController->ClientReceiveCompressedVoicePacket(PlayerState, CompressedVoiceData, Audience);
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
	const EVoiceChatAudience Audience)
{
	if (!IsLocalController() || !IsValid(SpeakerPlayerState) || CompressedVoiceData.IsEmpty())
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
		SteamVoiceComponent->PlayCompressedVoicePacket(SpeakerPlayerState, CompressedVoiceData);
	}

	ReceiveCompressedVoicePacket(SpeakerPlayerState, CompressedVoiceData);
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

void AShooterPlayerController::Input_ToggleInGameMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UTeamMatchUIInitComponent* UIInitComponent = GetTeamMatchUIInitComponent())
	{
		UIInitComponent->ToggleInGameMenu();
	}
}

void AShooterPlayerController::SetSystemMappingContextEnabled(const bool bEnabled)
{
	if (!IsValid(InputConfig))
	{
		return;
	}

	SetMappingContextEnabled(
		InputConfig->SystemMappingContext,
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
	StartVoiceTalking();
}

void AShooterPlayerController::Input_PushToTalkReleased()
{
	StopVoiceTalking();
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
