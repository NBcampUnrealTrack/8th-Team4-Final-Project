#include "GameMode/BaseRoundGameMode.h"

#include "Economy/CreditEconomyManager.h"
#include "GameMode/AugmentDraftManager.h"
#include "BaseRoundGameModeLog.h"
#include "GameMode/BaseRoundGameState.h"
#include "GameMode/TeamPlayerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
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
	CreditEconomyManager = CreateDefaultSubobject<UCreditEconomyManager>(TEXT("CreditEconomyManager"));
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
	NotifyPlayerEliminatedBy(EliminatedController, nullptr);
}

void ABaseRoundGameMode::NotifyPlayerEliminatedBy(
	AController* EliminatedController,
	AShooterPlayerState* KillerPlayerState)
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

	const bool bEnemyKill = IsValid(KillerPlayerState)
		&& KillerPlayerState != EliminatedPlayerState
		&& PlayerManager->IsRoundParticipant(KillerPlayerState)
		&& KillerPlayerState->GetTeam() != EGameTeam::None
		&& EliminatedPlayerState->GetTeam() != EGameTeam::None
		&& KillerPlayerState->GetTeam() != EliminatedPlayerState->GetTeam();

	if (bEnemyKill)
	{
		UCreditEconomyManager* Economy = GetCreditEconomyManager();
		if (!IsValid(Economy) || !Economy->RecordKillReward(KillerPlayerState))
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("Failed to record a kill reward for %s."),
				*GetNameSafe(KillerPlayerState));
		}
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

	if (!ResetEconomyForNewMatch())
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("Cannot restart game flow because credit reset failed."));
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
		UCreditEconomyManager* Economy = GetCreditEconomyManager();
		if (!IsValid(Economy) || !Economy->InitializePlayerForMatch(PlayerState))
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("Failed to initialize credits for %s."),
				*GetNameSafe(PlayerState));
		}

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
	SettleCompletedRoundCredits(LastCompletedRoundNumber, WinningTeam);

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

void ABaseRoundGameMode::RecordRoleObjectiveReward(const ETeamRole ObjectiveRole)
{
	const EGameTeam Team = GetTeamForRole(ObjectiveRole);
	if (Team == EGameTeam::None)
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("Cannot resolve the team for role %s while recording an objective reward."),
			*UEnum::GetValueAsString(ObjectiveRole));
		return;
	}

	UCreditEconomyManager* Economy = GetCreditEconomyManager();
	if (!IsValid(Economy))
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("Credit economy is unavailable for the %s objective reward."),
			*UEnum::GetValueAsString(Team));
		return;
	}

	if (!Economy->RecordTeamObjectiveReward(Team))
	{
		// 라운드 종료와 겹친 늦은 목표 통지는 게임 결과를 되돌리지 않고 보상만 폐기한다.
		UE_LOG(LogBaseRoundGameMode, Warning, TEXT("Could not record an objective reward for %s."),
			*UEnum::GetValueAsString(Team));
	}
}

bool ABaseRoundGameMode::InitializeRoundParticipantCredits()
{
	UCreditEconomyManager* Economy = GetCreditEconomyManager();
	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	if (!IsValid(Economy) || !IsValid(PlayerManager))
	{
		return false;
	}

	TArray<AShooterPlayerState*> Participants;
	PlayerManager->GetRoundParticipants(Participants);
	if (Participants.IsEmpty())
	{
		return false;
	}

	for (AShooterPlayerState* PlayerState : Participants)
	{
		if (!Economy->InitializePlayerForMatch(PlayerState))
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("Failed to initialize round credits for %s."),
				*GetNameSafe(PlayerState));
			return false;
		}
	}

	return true;
}

bool ABaseRoundGameMode::ResetEconomyForNewMatch()
{
	UCreditEconomyManager* Economy = GetCreditEconomyManager();
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(Economy) || !IsValid(RoundGameState))
	{
		return false;
	}

	TArray<AShooterPlayerState*> MatchPlayers;
	MatchPlayers.Reserve(RoundGameState->PlayerArray.Num());
	for (APlayerState* PlayerState : RoundGameState->PlayerArray)
	{
		if (AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState))
		{
			MatchPlayers.Add(ShooterPlayerState);
		}
	}

	return Economy->ResetForNewMatch(MatchPlayers);
}

void ABaseRoundGameMode::SettleCompletedRoundCredits(const int32 RoundNumber, const EGameTeam WinningTeam)
{
	UCreditEconomyManager* Economy = GetCreditEconomyManager();
	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	if (!IsValid(Economy) || !IsValid(PlayerManager))
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("Cannot settle credits for round %d: dependency missing."), RoundNumber);
		if (IsValid(Economy))
		{
			Economy->CancelActiveRound();
		}
		return;
	}

	TArray<AShooterPlayerState*> Participants;
	PlayerManager->GetRoundParticipants(Participants);
	if (!Economy->SettleRoundCredits(RoundNumber, WinningTeam, Participants))
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("Credit settlement failed for round %d."), RoundNumber);
		Economy->CancelActiveRound();
	}
}

ABaseRoundGameState* ABaseRoundGameMode::GetBaseRoundGameState() const
{
	return GetGameState<ABaseRoundGameState>();
}


UCreditEconomyManager* ABaseRoundGameMode::GetCreditEconomyManager() const
{
	return CreditEconomyManager.Get();
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
