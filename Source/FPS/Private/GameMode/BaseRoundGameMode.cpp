#include "GameMode/BaseRoundGameMode.h"

#include "GameMode/AugmentDraftManager.h"
#include "BaseRoundGameModeLog.h"
#include "GameMode/BaseRoundGameState.h"
#include "GameMode/TeamPlayerManager.h"
#include "GameFramework/PlayerController.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogBaseRoundGameMode);

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
	InitialPlayerReadinessTracker.RemovePlayer(Cast<APlayerController>(Exiting));

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

	if (InitialPlayerReadinessTracker.MarkPlayerReady(PlayerController))
	{
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
	InitialPlayerReadinessTracker.ResetTimeoutForSameMapRestart();
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
			&& !InitialPlayerReadinessTracker.HasTimeoutExpired()
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
