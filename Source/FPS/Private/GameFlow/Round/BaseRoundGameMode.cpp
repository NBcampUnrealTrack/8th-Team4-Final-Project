#include "GameFlow/Round/BaseRoundGameMode.h"

#include "Engine/World.h"
#include "GameFlow/Common/BaseTeamPlayerState.h"
#include "GameFlow/Common/TeamPlayerManager.h"
#include "GameFlow/Round/AugmentDraftManager.h"
#include "GameFlow/Round/BaseRoundGameState.h"
#include "GameFlow/Round/BaseRoundPlayerController.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogBaseRoundGameMode, Log, All);

/*
 * 첫 라운드: WaitingForPlayers -> WaitingForPlayersReady -> RoundStarting
 * 이후 라운드: WaitingForPlayers -> RoundStarting
 * RoundStarting -> OpenNextRound -> AugmentSelecting or RoundInProgress
 * AugmentSelecting -> FinishAugmentDraft -> RoundInProgress
 * RoundInProgress -> EndRound -> RoundEnding or MatchEnded
 * RoundEnding -> AdvanceToNextRound -> WaitingForPlayers
 */

ABaseRoundGameMode::ABaseRoundGameMode()
{
	GameStateClass = ABaseRoundGameState::StaticClass();
	bDelayedStart = true;
	AugmentDraftManager = CreateDefaultSubobject<UAugmentDraftManager>(TEXT("AugmentDraftManager"));
}

void ABaseRoundGameMode::Logout(AController* Exiting)
{
	InitialReadyPlayerControllers.Remove(TWeakObjectPtr<APlayerController>(Cast<APlayerController>(Exiting)));

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	const EGameFlowPhase Phase = IsValid(RoundGameState)
		? RoundGameState->GetGameFlowPhase()
		: EGameFlowPhase::WaitingForPlayers;
	AShooterPlayerState* ExitingPlayerState = IsValid(Exiting)
		? Exiting->GetPlayerState<AShooterPlayerState>()
		: nullptr;

	if (Phase == EGameFlowPhase::RoundInProgress)
	{
		NotifyPlayerEliminated(Exiting);
	}
	else if (Phase == EGameFlowPhase::AugmentSelecting)
	{
		const UAugmentDraftManager* DraftManager = GetAugmentDraftManager();
		if (IsValid(DraftManager) && DraftManager->IsParticipant(ExitingPlayerState))
		{
			// PlayerState와 ASC가 유효할 때 드래프트를 먼저 정리한다.
			EnterWaitingForPlayers();
		}
	}

	Super::Logout(Exiting);
	UpdateInitialLoadingStatus();
	HandlePlayerRosterChanged();
}

void ABaseRoundGameMode::NotifyPlayerReadyForGameFlow(APlayerController* PlayerController)
{
	if (!HasAuthority()
		|| !IsValid(PlayerController)
		|| !PlayerController->HasAuthority()
		|| PlayerController->GetWorld() != GetWorld())
	{
		return;
	}

	const TWeakObjectPtr<APlayerController> ReadyController(PlayerController);
	if (!InitialReadyPlayerControllers.Contains(ReadyController))
	{
		InitialReadyPlayerControllers.Add(ReadyController);
		UE_LOG(LogBaseRoundGameMode, Display, TEXT("%s reported that its in-game UI is ready."), *GetNameSafe(PlayerController));
	}

	UpdateInitialLoadingStatus();
	TryBeginNextRound();
}

void ABaseRoundGameMode::NotifyPlayerEliminated(AController* EliminatedController)
{
	if (!HasAuthority() || !IsValid(EliminatedController))
	{
		return;
	}

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundInProgress)
	{
		return;
	}

	UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	ABaseTeamPlayerState* EliminatedPlayerState = nullptr;
	if (!IsValid(PlayerManager)
		|| !PlayerManager->TryMarkPlayerEliminated(EliminatedController, EliminatedPlayerState))
	{
		return;
	}

	SetPlayerRoundInputEnabled(Cast<APlayerController>(EliminatedController), false);
	HandleModePlayerEliminated(EliminatedPlayerState);
}

void ABaseRoundGameMode::RestartGameFlow()
{
	if (!HasAuthority())
	{
		return;
	}

	if (HasMatchEnded())
	{
		RestartGame();
		return;
	}

	ClearAllFlowTimers();
	SetAllPlayersRoundInputEnabled(false);
	PendingRoundContext.Reset();
	LastCompletedRoundNumber = 0;
	SpawnPreparationRetryCount = 0;
	bInitialReadyTimeoutExpired = false;
	bRoundOpeningSuspended = false;
	bLoggedWaitingForAugmentRegistry = false;

	if (UAugmentDraftManager* DraftManager = GetAugmentDraftManager())
	{
		DraftManager->ResetForNewMatch();
	}

	ResetModeState();
	if (UTeamPlayerManager* PlayerManager = GetTeamPlayerManager())
	{
		PlayerManager->ResetAllPlayerPawns();
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	RoundGameState->ResetTeamScores();
	RoundGameState->SetCurrentRoundNumber(0);
	RoundGameState->SetTeamRoles(StartingTeamARole, StartingTeamBRole);
	RoundGameState->ClearRoundResult();
	RoundGameState->SetMatchWinner(EGameTeam::None);
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);
	UpdateInitialLoadingStatus();
	TryBeginNextRound();
}

void ABaseRoundGameMode::RetryRoundOpening()
{
	if (!HasAuthority() || HasMatchEnded())
	{
		return;
	}

	bRoundOpeningSuspended = false;
	SpawnPreparationRetryCount = 0;
	TryBeginNextRound();
}

void ABaseRoundGameMode::BeginPlay()
{
	Super::BeginPlay();
	InitializeGameFlow();
}

void ABaseRoundGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllFlowTimers();
	PendingRoundContext.Reset();

	if (UAugmentDraftManager* DraftManager = GetAugmentDraftManager())
	{
		DraftManager->OnDependenciesReady.RemoveAll(this);
		DraftManager->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}

void ABaseRoundGameMode::InitGameState()
{
	Super::InitGameState();

	if (ABaseRoundGameState* RoundGameState = GetBaseRoundGameState())
	{
		RoundGameState->InitializeRoundConfiguration(MaxRounds);
		RoundGameState->SetTeamRoles(StartingTeamARole, StartingTeamBRole);
	}
}

void ABaseRoundGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!IsValid(NewPlayer) || !InitializeTeamPlayer(NewPlayer))
	{
		return;
	}

	// 라운드 모드는 다음 라운드를 열 때 참가자를 한 번에 스폰한다.
	AShooterPlayerState* PlayerState = NewPlayer->GetPlayerState<AShooterPlayerState>();
	if (IsValid(PlayerState))
	{
		PlayerState->SetAlive(false);
		if (UAugmentDraftManager* DraftManager = GetAugmentDraftManager())
		{
			const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
			const EGameFlowPhase Phase = IsValid(RoundGameState)
				? RoundGameState->GetGameFlowPhase()
				: EGameFlowPhase::WaitingForPlayers;
			const bool bJoinedBeforeFirstRound = LastCompletedRoundNumber == 0
				&& (Phase == EGameFlowPhase::WaitingForPlayers
					|| Phase == EGameFlowPhase::WaitingForPlayersReady);
			if (bJoinedBeforeFirstRound)
			{
				DraftManager->ResetPlayerForNewMatch(PlayerState);
			}
			DraftManager->SynchronizeGameplayInputBlock(PlayerState);
		}
	}

	if (const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager())
	{
		PlayerManager->RemovePlayerPawn(NewPlayer);
	}
	SetPlayerRoundInputEnabled(NewPlayer, false);
	UpdateInitialLoadingStatus();
	HandlePlayerRosterChanged();
}

bool ABaseRoundGameMode::ReadyToStartMatch_Implementation()
{
	return false;
}

bool ABaseRoundGameMode::ReadyToEndMatch_Implementation()
{
	return false;
}

bool ABaseRoundGameMode::ShouldSpawnAtStartSpot(AController*)
{
	return false;
}

void ABaseRoundGameMode::ResetModeState()
{
}

EPlayerStartReservationResult ABaseRoundGameMode::ReserveModePlayerStarts(const FRoundContext&)
{
	if (UTeamPlayerManager* PlayerManager = GetTeamPlayerManager())
	{
		PlayerManager->ClearPlayerStartReservations();
	}
	return EPlayerStartReservationResult::Succeeded;
}

bool ABaseRoundGameMode::PrepareModeRound(const FRoundContext&)
{
	return true;
}

bool ABaseRoundGameMode::ActivateModeRound()
{
	return true;
}

void ABaseRoundGameMode::DeactivateModeRound(const EGameTeam, const ERoundEndReason)
{
}

void ABaseRoundGameMode::HandleModePlayerEliminated(ABaseTeamPlayerState*)
{
}

void ABaseRoundGameMode::ResolveModeRoundTimeExpired()
{
}

bool ABaseRoundGameMode::HasModeRequiredPlayers() const
{
	return true;
}

bool ABaseRoundGameMode::IsModeConfigurationValid() const
{
	return true;
}

void ABaseRoundGameMode::EndRound(const EGameTeam WinningTeam, const ERoundEndReason EndReason)
{
	if (!HasAuthority() || WinningTeam == EGameTeam::None)
	{
		return;
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundInProgress)
	{
		return;
	}

	ClearRoundTimer();
	SetRoundParticipantInputEnabled(false);
	DeactivateModeRound(WinningTeam, EndReason);

	RoundGameState->AddTeamScore(WinningTeam);
	RoundGameState->SetRoundResult(WinningTeam, EndReason);
	LastCompletedRoundNumber = RoundGameState->GetCurrentRoundNumber();

	EGameTeam MatchWinner = EGameTeam::None;
	if (IsMatchComplete(MatchWinner))
	{
		FinishMatch(MatchWinner);
		return;
	}

	RoundGameState->SetGameFlowPhase(EGameFlowPhase::RoundEnding, RoundEndDelay);
	if (RoundEndDelay <= 0.0f)
	{
		AdvanceToNextRound();
		return;
	}

	GetWorldTimerManager().SetTimer(
		PhaseTimerHandle,
		this,
		&ABaseRoundGameMode::AdvanceToNextRound,
		RoundEndDelay,
		false);
}

void ABaseRoundGameMode::FreezeRoundTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RoundTimerHandle);
	if (ABaseRoundGameState* RoundGameState = GetBaseRoundGameState())
	{
		RoundGameState->FreezeRoundTimer();
	}
}

ABaseRoundGameState* ABaseRoundGameMode::GetBaseRoundGameState() const
{
	return GetGameState<ABaseRoundGameState>();
}

ETeamRole ABaseRoundGameMode::GetRoleForTeam(const EGameTeam Team) const
{
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	return IsValid(RoundGameState) ? RoundGameState->GetTeamRole(Team) : ETeamRole::None;
}

EGameTeam ABaseRoundGameMode::GetTeamForRole(const ETeamRole TeamRole) const
{
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	return IsValid(RoundGameState) ? RoundGameState->GetTeamForRole(TeamRole) : EGameTeam::None;
}

ETeamRole ABaseRoundGameMode::GetSpawnRoleForTeam(const EGameTeam Team) const
{
	return PendingRoundContext.IsSet()
		? PendingRoundContext.GetValue().GetRoleForTeam(Team)
		: GetRoleForTeam(Team);
}

void ABaseRoundGameMode::InitializeGameFlow()
{
	UAugmentDraftManager* DraftManager = GetAugmentDraftManager();
	if (IsValid(DraftManager))
	{
		DraftManager->Initialize(this, bEnableAugmentDraft);
		DraftManager->OnDependenciesReady.AddUObject(this, &ABaseRoundGameMode::HandleDraftDependenciesReady);
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	LastCompletedRoundNumber = RoundGameState->GetCurrentRoundNumber();
	InitialReadyPlayerControllers.Reset();
	bInitialReadyTimeoutExpired = false;
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);
	UpdateInitialLoadingStatus();

	TryBeginNextRound();
}

void ABaseRoundGameMode::HandlePlayerRosterChanged()
{
	if (!HasAuthority())
	{
		return;
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || RoundGameState->GetGameFlowPhase() == EGameFlowPhase::MatchEnded)
	{
		return;
	}

	const EGameFlowPhase Phase = RoundGameState->GetGameFlowPhase();
	if (Phase == EGameFlowPhase::WaitingForPlayers
		|| Phase == EGameFlowPhase::WaitingForPlayersReady)
	{
		TryBeginNextRound();
		return;
	}

	if (Phase == EGameFlowPhase::RoundStarting)
	{
		if (!HasRequiredPlayersToStart())
		{
			EnterWaitingForPlayers();
			return;
		}

		// 첫 라운드 시작 지연 중 새 플레이어가 합류했다면 그 플레이어의 UI 준비도 기다린다.
		if (LastCompletedRoundNumber == 0
			&& !bInitialReadyTimeoutExpired
			&& !AreInitialPlayersReadyForGameFlow())
		{
			EnterWaitingForPlayersReady();
			return;
		}
	}

	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	if (Phase == EGameFlowPhase::AugmentSelecting
		&& (!IsValid(PlayerManager) || !PlayerManager->AreRoundParticipantsReady()))
	{
		EnterWaitingForPlayers();
		TryBeginNextRound();
	}
}

void ABaseRoundGameMode::HandleDraftDependenciesReady()
{
	if (!HasAuthority())
	{
		return;
	}

	bLoggedWaitingForAugmentRegistry = false;
	TryBeginNextRound();
}

void ABaseRoundGameMode::TryBeginNextRound()
{
	if (!HasAuthority() || bRoundOpeningSuspended)
	{
		return;
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	const EGameFlowPhase Phase = RoundGameState->GetGameFlowPhase();
	if (Phase != EGameFlowPhase::WaitingForPlayers
		&& Phase != EGameFlowPhase::WaitingForPlayersReady)
	{
		return;
	}

	if (!IsGameFlowConfigurationValid())
	{
		SuspendRoundOpening(TEXT("The round GameMode configuration is invalid."));
		return;
	}

	if (!HasRequiredPlayersToStart())
	{
		if (Phase == EGameFlowPhase::WaitingForPlayersReady)
		{
			EnterWaitingForPlayers();
		}
		return;
	}

	const bool bNeedsInitialReadyGate = LastCompletedRoundNumber == 0;
	if (bNeedsInitialReadyGate && Phase == EGameFlowPhase::WaitingForPlayers)
	{
		EnterWaitingForPlayersReady();
	}

	const UAugmentDraftManager* DraftManager = GetAugmentDraftManager();
	if (bEnableAugmentDraft && (!IsValid(DraftManager) || !DraftManager->HasRequiredDependencies()))
	{
		SuspendRoundOpening(TEXT("The augment draft subsystem is missing."));
		return;
	}

	if (bEnableAugmentDraft && !DraftManager->AreDependenciesReady())
	{
		if (!bLoggedWaitingForAugmentRegistry)
		{
			bLoggedWaitingForAugmentRegistry = true;
			UE_LOG(LogBaseRoundGameMode, Display, TEXT("Waiting for augment registry loading to finish."));
		}
		return;
	}

	if (bNeedsInitialReadyGate
		&& !bInitialReadyTimeoutExpired
		&& !AreInitialPlayersReadyForGameFlow())
	{
		return;
	}

	bLoggedWaitingForAugmentRegistry = false;
	ClearInitialReadyTimer();
	ClearPhaseTimer();
	SetAllPlayersRoundInputEnabled(false);

	const float Delay = LastCompletedRoundNumber == 0 ? InitialRoundStartDelay : RoundStartDelay;
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::RoundStarting, Delay);
	if (Delay <= 0.0f)
	{
		OpenNextRound();
		return;
	}

	GetWorldTimerManager().SetTimer(
		PhaseTimerHandle,
		this,
		&ABaseRoundGameMode::OpenNextRound,
		Delay,
		false);
}

void ABaseRoundGameMode::OpenNextRound()
{
	ClearPhaseTimer();

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundStarting)
	{
		return;
	}

	if (!IsGameFlowConfigurationValid())
	{
		SuspendRoundOpening(TEXT("The round GameMode configuration became invalid before round opening."));
		return;
	}

	if (!HasRequiredPlayersToStart())
	{
		EnterWaitingForPlayers();
		return;
	}

	const FRoundContext RoundContext = MakeRoundContext(LastCompletedRoundNumber + 1);
	if (!RoundContext.IsValid())
	{
		SuspendRoundOpening(TEXT("The next round context is invalid."));
		return;
	}

	// Pending Context는 공수별 스폰을 포함한 서버 준비 단계에서만 사용한다.
	PendingRoundContext = RoundContext;

	UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	if (!IsValid(PlayerManager))
	{
		SuspendRoundOpening(TEXT("The team player manager is missing."));
		return;
	}

	const EPlayerStartReservationResult StartReservationResult = ReserveModePlayerStarts(RoundContext);
	if (StartReservationResult == EPlayerStartReservationResult::InvalidConfiguration)
	{
		SuspendRoundOpening(TEXT("The mode-specific PlayerStart configuration is invalid."));
		return;
	}
	if (StartReservationResult == EPlayerStartReservationResult::TemporarilyBlocked)
	{
		ScheduleSpawnPreparationRetry(TEXT("Required PlayerStarts are temporarily blocked."));
		return;
	}

	const ERoundPlayerPreparationResult PlayerPreparationResult = PlayerManager->PreparePlayersForRound();
	if (PlayerPreparationResult == ERoundPlayerPreparationResult::TemporarilyBlocked)
	{
		ScheduleSpawnPreparationRetry(TEXT("A reserved PlayerStart could not spawn its pawn."));
		return;
	}
	if (PlayerPreparationResult != ERoundPlayerPreparationResult::Succeeded)
	{
		SuspendRoundOpening(TEXT("Player spawn or state reset failed with a non-recoverable error."));
		return;
	}
	SpawnPreparationRetryCount = 0;

	if (!PrepareModeRound(RoundContext))
	{
		SuspendRoundOpening(TEXT("Mode-specific round preparation failed."));
		return;
	}

	UAugmentDraftManager* DraftManager = nullptr;
	if (bEnableAugmentDraft)
	{
		TArray<AShooterPlayerState*> Participants;
		PlayerManager->GetRoundParticipants(Participants);

		DraftManager = GetAugmentDraftManager();
		if (!IsValid(DraftManager)
			|| !DraftManager->PrepareDraft(RoundContext.RoundNumber, AugmentOfferCount, Participants))
		{
			SuspendRoundOpening(TEXT("Augment draft preparation failed."));
			return;
		}
	}

	// 모든 라운드 리소스가 준비된 뒤에만 새 라운드 정보를 공개한다.
	RoundGameState->SetRoundContext(RoundContext);
	if (bEnableAugmentDraft)
	{
		// 서버 페이즈를 먼저 전환한 뒤 선택 RPC 게이트를 연다.
		RoundGameState->SetGameFlowPhase(EGameFlowPhase::AugmentSelecting, AugmentSelectionDuration);
		if (!DraftManager->OpenDraft())
		{
			SuspendRoundOpening(TEXT("Augment draft activation failed."));
			return;
		}

		SetRoundParticipantInputEnabled(true);
		GetWorldTimerManager().SetTimer(
			PhaseTimerHandle,
			this,
			&ABaseRoundGameMode::FinishAugmentDraft,
			AugmentSelectionDuration,
			false);
		return;
	}

	if (!TryStartRoundCombat())
	{
		SuspendRoundOpening(TEXT("The round could not enter combat."));
	}
}

void ABaseRoundGameMode::FinishAugmentDraft()
{
	ClearPhaseTimer();

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || RoundGameState->GetGameFlowPhase() != EGameFlowPhase::AugmentSelecting)
	{
		return;
	}

	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	if (!IsValid(PlayerManager) || !PlayerManager->AreRoundParticipantsReady())
	{
		EnterWaitingForPlayers();
		TryBeginNextRound();
		return;
	}

	if (!TryStartRoundCombat())
	{
		SuspendRoundOpening(TEXT("Draft completion or mode activation failed."));
	}
}

bool ABaseRoundGameMode::TryStartRoundCombat()
{
	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	const EGameFlowPhase ExpectedPhase = bEnableAugmentDraft
		? EGameFlowPhase::AugmentSelecting
		: EGameFlowPhase::RoundStarting;
	if (!IsValid(RoundGameState)
		|| RoundGameState->GetGameFlowPhase() != ExpectedPhase
		|| !IsValid(PlayerManager)
		|| !PendingRoundContext.IsSet()
		|| !PendingRoundContext.GetValue().IsValid()
		|| !PlayerManager->AreRoundParticipantsReady())
	{
		return false;
	}

	// Draft 태그가 먼저 복제돼도 전투 페이즈 공개 전에는 입력이 열리지 않게 한다.
	SetRoundParticipantInputEnabled(false);

	// 입력 차단 해제 뒤 실패 가능한 작업이 남지 않도록 모드를 먼저 활성화한다.
	if (!ActivateModeRound())
	{
		return false;
	}

	if (bEnableAugmentDraft)
	{
		UAugmentDraftManager* DraftManager = GetAugmentDraftManager();
		if (!IsValid(DraftManager) || !DraftManager->CompleteDraft())
		{
			return false;
		}
	}

	if (!HasMatchStarted())
	{
		StartMatch();
	}

	RoundGameState->SetGameFlowPhase(EGameFlowPhase::RoundInProgress);
	StartRoundTimer(RoundDuration);
	SetRoundParticipantInputEnabled(true);

	PendingRoundContext.Reset();
	return true;
}

void ABaseRoundGameMode::AdvanceToNextRound()
{
	ClearPhaseTimer();

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundEnding)
	{
		return;
	}

	EnterWaitingForPlayers();
	TryBeginNextRound();
}

void ABaseRoundGameMode::FinishMatch(const EGameTeam WinningTeam)
{
	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	ClearAllFlowTimers();
	SetAllPlayersRoundInputEnabled(false);
	PendingRoundContext.Reset();
	bRoundOpeningSuspended = false;

	if (UAugmentDraftManager* DraftManager = GetAugmentDraftManager())
	{
		DraftManager->CancelDraft();
	}

	RoundGameState->SetMatchWinner(WinningTeam);
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::MatchEnded);
	EndMatch();
}

void ABaseRoundGameMode::EnterWaitingForPlayers()
{
	ClearAllFlowTimers();
	SetAllPlayersRoundInputEnabled(false);
	SpawnPreparationRetryCount = 0;

	if (UAugmentDraftManager* DraftManager = GetAugmentDraftManager())
	{
		DraftManager->CancelDraft();
	}

	ResetModeState();
	if (UTeamPlayerManager* PlayerManager = GetTeamPlayerManager())
	{
		PlayerManager->ResetAllPlayerPawns();
	}

	PendingRoundContext.Reset();

	if (ABaseRoundGameState* RoundGameState = GetBaseRoundGameState())
	{
		const FRoundContext PublishedContext = MakeRoundContext(FMath::Max(1, LastCompletedRoundNumber));
		RoundGameState->SetCurrentRoundNumber(LastCompletedRoundNumber);
		RoundGameState->SetTeamRoles(PublishedContext.TeamARole, PublishedContext.TeamBRole);
		RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);
	}
}

void ABaseRoundGameMode::EnterWaitingForPlayersReady()
{
	ClearPhaseTimer();
	ClearInitialReadyTimer();
	SetAllPlayersRoundInputEnabled(false);
	SpawnPreparationRetryCount = 0;

	if (ABaseRoundGameState* RoundGameState = GetBaseRoundGameState())
	{
		RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayersReady);
		GetWorldTimerManager().SetTimer(
			InitialReadyTimerHandle,
			this,
			&ThisClass::HandleInitialReadyTimeout,
			InitialReadyTimeout,
			false);
	}
}

void ABaseRoundGameMode::HandleInitialReadyTimeout()
{
	ClearInitialReadyTimer();

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState)
		|| RoundGameState->GetGameFlowPhase() != EGameFlowPhase::WaitingForPlayersReady)
	{
		return;
	}

	if (AreInitialPlayersReadyForGameFlow())
	{
		TryBeginNextRound();
		return;
	}

	TArray<FString> PendingPlayerNames;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		const ABaseTeamPlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ABaseTeamPlayerState>()
			: nullptr;
		if (!IsValid(PlayerState)
			|| PlayerState->GetTeam() == EGameTeam::None
			|| InitialReadyPlayerControllers.Contains(TWeakObjectPtr<APlayerController>(PlayerController)))
		{
			continue;
		}

		const FString PlayerName = PlayerState->GetPlayerName();
		PendingPlayerNames.Add(PlayerName.IsEmpty() ? GetNameSafe(PlayerController) : PlayerName);
	}

	const FString PendingPlayerList = PendingPlayerNames.IsEmpty()
		? TEXT("<unknown>")
		: FString::Join(PendingPlayerNames, TEXT(", "));
	UE_LOG(LogBaseRoundGameMode, Warning,
		TEXT("Initial player ready gate timed out after %.1f seconds. Continuing while UI initialization keeps retrying. Unready players: %s"),
		InitialReadyTimeout,
		*PendingPlayerList);
	bInitialReadyTimeoutExpired = true;
	TryBeginNextRound();
}

void ABaseRoundGameMode::ScheduleSpawnPreparationRetry(const TCHAR* Reason)
{
	if (UTeamPlayerManager* PlayerManager = GetTeamPlayerManager())
	{
		PlayerManager->ClearPlayerStartReservations();
	}
	PendingRoundContext.Reset();

	if (SpawnPreparationRetryCount >= MaxSpawnPreparationRetries)
	{
		UE_LOG(LogBaseRoundGameMode, Error,
			TEXT("Round spawn retry limit reached after %d retries. Last failure: %s"),
			SpawnPreparationRetryCount,
			Reason);
		SuspendRoundOpening(TEXT("Temporary PlayerStart blockage did not clear before the spawn retry limit."));
		return;
	}

	++SpawnPreparationRetryCount;
	UE_LOG(LogBaseRoundGameMode, Warning,
		TEXT("Round spawn preparation retry %d/%d in %.2f seconds: %s"),
		SpawnPreparationRetryCount,
		MaxSpawnPreparationRetries,
		SpawnPreparationRetryDelay,
		Reason);

	GetWorldTimerManager().SetTimer(
		PhaseTimerHandle,
		this,
		&ThisClass::OpenNextRound,
		SpawnPreparationRetryDelay,
		false);
}

void ABaseRoundGameMode::SuspendRoundOpening(const TCHAR* Reason)
{
	bRoundOpeningSuspended = true;
	UE_LOG(LogBaseRoundGameMode, Error,
		TEXT("Round opening suspended: %s Fix the reported configuration/runtime cause and call RetryRoundOpening()."),
		Reason);
	EnterWaitingForPlayers();
}

FRoundContext ABaseRoundGameMode::MakeRoundContext(const int32 RoundNumber) const
{
	return FRoundContext::Make(
		RoundNumber,
		SideSwitchRound,
		StartingTeamARole,
		StartingTeamBRole);
}

bool ABaseRoundGameMode::HasRequiredPlayersToStart() const
{
	const int32 AssignedPlayerCount = GetTeamPlayerCount(EGameTeam::TeamA)
		+ GetTeamPlayerCount(EGameTeam::TeamB);
	return AssignedPlayerCount >= MinimumPlayersToStart && HasModeRequiredPlayers();
}

bool ABaseRoundGameMode::AreInitialPlayersReadyForGameFlow() const
{
	int32 AssignedPlayerCount = 0;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		const ABaseTeamPlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ABaseTeamPlayerState>()
			: nullptr;
		if (!IsValid(PlayerState) || PlayerState->GetTeam() == EGameTeam::None)
		{
			continue;
		}

		++AssignedPlayerCount;
		if (!InitialReadyPlayerControllers.Contains(TWeakObjectPtr<APlayerController>(PlayerController)))
		{
			return false;
		}
	}

	return AssignedPlayerCount >= MinimumPlayersToStart;
}

void ABaseRoundGameMode::UpdateInitialLoadingStatus()
{
	if (!HasAuthority())
	{
		return;
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	int32 ReadyPlayerCount = 0;
	int32 RequiredPlayerCount = 0;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		ABaseTeamPlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ABaseTeamPlayerState>()
			: nullptr;
		if (!IsValid(PlayerState) || PlayerState->GetTeam() == EGameTeam::None)
		{
			continue;
		}

		++RequiredPlayerCount;
		const bool bIsReady = InitialReadyPlayerControllers.Contains(
			TWeakObjectPtr<APlayerController>(PlayerController));
		ReadyPlayerCount += bIsReady ? 1 : 0;

		if (AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState))
		{
			ShooterPlayerState->SetLoadedIntoMatch(bIsReady);
		}
	}

	RoundGameState->SetLoadingStatus(ReadyPlayerCount, RequiredPlayerCount);
}

bool ABaseRoundGameMode::IsBaseConfigurationValid() const
{
	const bool bRolesAreValid = StartingTeamARole != ETeamRole::None
		&& StartingTeamBRole != ETeamRole::None
		&& StartingTeamARole != StartingTeamBRole;

	return DefaultPawnClass != nullptr
		&& !DefaultPawnClass->IsChildOf(ADefaultPawn::StaticClass())
		&& !DefaultPawnClass->HasAnyClassFlags(CLASS_Abstract)
		&& PlayerControllerClass != nullptr
		&& PlayerControllerClass->IsChildOf(ABaseRoundPlayerController::StaticClass())
		&& !PlayerControllerClass->HasAnyClassFlags(CLASS_Abstract)
		&& GameStateClass != nullptr
		&& GameStateClass->IsChildOf(ABaseRoundGameState::StaticClass())
		&& !GameStateClass->HasAnyClassFlags(CLASS_Abstract)
		&& PlayerStateClass != nullptr
		&& PlayerStateClass->IsChildOf(AShooterPlayerState::StaticClass())
		&& !PlayerStateClass->HasAnyClassFlags(CLASS_Abstract)
		&& SpawnClearanceRadius >= 0.0f
		&& MinimumPlayersToStart > 0
		&& InitialRoundStartDelay >= 0.0f
		&& InitialReadyTimeout > 0.0f
		&& RoundStartDelay >= 0.0f
		&& RoundEndDelay >= 0.0f
		&& MaxSpawnPreparationRetries >= 0
		&& SpawnPreparationRetryDelay > 0.0f
		&& RoundDuration > 0.0f
		&& (!bEnableAugmentDraft || (AugmentSelectionDuration > 0.0f && AugmentOfferCount > 0))
		&& SideSwitchRound >= 0
		&& RoundsToWin >= 0
		&& MaxRounds >= 0
		&& bRolesAreValid;
}

bool ABaseRoundGameMode::IsGameFlowConfigurationValid() const
{
	return IsBaseConfigurationValid() && IsModeConfigurationValid();
}

bool ABaseRoundGameMode::IsMatchComplete(EGameTeam& OutWinningTeam) const
{
	OutWinningTeam = EGameTeam::None;

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return false;
	}

	const int32 TeamAScore = RoundGameState->GetTeamAScore();
	const int32 TeamBScore = RoundGameState->GetTeamBScore();
	if (RoundsToWin > 0 && (TeamAScore >= RoundsToWin || TeamBScore >= RoundsToWin))
	{
		OutWinningTeam = TeamAScore > TeamBScore ? EGameTeam::TeamA : EGameTeam::TeamB;
		return true;
	}

	if (MaxRounds > 0 && LastCompletedRoundNumber >= MaxRounds)
	{
		OutWinningTeam = RoundGameState->GetLeadingTeam();
		return true;
	}

	return false;
}

void ABaseRoundGameMode::StartRoundTimer(const float Duration)
{
	ClearRoundTimer();

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	RoundGameState->SetRoundTimer(Duration);
	if (Duration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			RoundTimerHandle,
			this,
			&ABaseRoundGameMode::HandleRoundTimerExpired,
			Duration,
			false);
	}
}

void ABaseRoundGameMode::HandleRoundTimerExpired()
{
	GetWorldTimerManager().ClearTimer(RoundTimerHandle);

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (IsValid(RoundGameState) && RoundGameState->GetGameFlowPhase() == EGameFlowPhase::RoundInProgress)
	{
		ResolveModeRoundTimeExpired();
	}
}

void ABaseRoundGameMode::ClearPhaseTimer()
{
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
}

void ABaseRoundGameMode::ClearRoundTimer()
{
	GetWorldTimerManager().ClearTimer(RoundTimerHandle);
	if (ABaseRoundGameState* RoundGameState = GetBaseRoundGameState())
	{
		RoundGameState->ClearRoundTimer();
	}
}

void ABaseRoundGameMode::ClearInitialReadyTimer()
{
	GetWorldTimerManager().ClearTimer(InitialReadyTimerHandle);
}

void ABaseRoundGameMode::ClearAllFlowTimers()
{
	ClearPhaseTimer();
	ClearRoundTimer();
	ClearInitialReadyTimer();
}

void ABaseRoundGameMode::SetAllPlayersRoundInputEnabled(const bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		SetPlayerRoundInputEnabled(Iterator->Get(), bEnabled);
	}
}

void ABaseRoundGameMode::SetRoundParticipantInputEnabled(const bool bEnabled)
{
	TArray<APlayerController*> Participants;
	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	if (!IsValid(PlayerManager))
	{
		return;
	}
	PlayerManager->GetRoundParticipantControllers(Participants);
	for (APlayerController* PlayerController : Participants)
	{
		SetPlayerRoundInputEnabled(PlayerController, bEnabled);
	}
}

void ABaseRoundGameMode::SetPlayerRoundInputEnabled(
	APlayerController* PlayerController,
	const bool bEnabled) const
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	ABaseRoundPlayerController* RoundPlayerController = Cast<ABaseRoundPlayerController>(PlayerController);
	if (!IsValid(RoundPlayerController))
	{
		UE_LOG(LogBaseRoundGameMode, Error,
			TEXT("%s is not derived from ABaseRoundPlayerController; round input cannot be synchronized."),
			*GetNameSafe(PlayerController));
		return;
	}

	RoundPlayerController->SetRoundInputEnabled(bEnabled);
}

UAugmentDraftManager* ABaseRoundGameMode::GetAugmentDraftManager() const
{
	return AugmentDraftManager.Get();
}
