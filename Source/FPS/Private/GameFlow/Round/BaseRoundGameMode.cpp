#include "GameFlow/Round/BaseRoundGameMode.h"

#include "Engine/World.h"
#include "GameFlow/Common/BaseTeamPlayerState.h"
#include "GameFlow/Round/BaseRoundGameState.h"
#include "GameFlow/Round/BaseRoundPlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogBaseRoundGameMode, Log, All);

ABaseRoundGameMode::ABaseRoundGameMode()
{
	GameStateClass = ABaseRoundGameState::StaticClass();
	bDelayedStart = true;
}

void ABaseRoundGameMode::Logout(AController* Exiting)
{
	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (IsValid(RoundGameState) && RoundGameState->GetGameFlowPhase() == EGameFlowPhase::RoundInProgress)
	{
		NotifyPlayerEliminated(Exiting);
	}

	Super::Logout(Exiting);
	EvaluateRoundStart();
}

void ABaseRoundGameMode::NotifyPlayerEliminated(AController* EliminatedController)
{
	if (!HasAuthority() || !IsValid(EliminatedController))
	{
		return;
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundInProgress)
	{
		return;
	}

	ABaseTeamPlayerState* EliminatedPlayerState = EliminatedController->GetPlayerState<ABaseTeamPlayerState>();
	if (!IsValid(EliminatedPlayerState) || !EliminatedPlayerState->IsAlive())
	{
		return;
	}

	EliminatedPlayerState->SetAlive(false);
	HandlePlayerEliminated(EliminatedPlayerState);
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

	GetWorldTimerManager().ClearTimer(RoundStartTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundTimeTimerHandle);
	SetRoundPlayerInputEnabled(false);
	HandleGameFlowReset();

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	ResetAllPlayerPawns();
	RoundGameState->ResetTeamScores();
	RoundGameState->SetCurrentRoundNumber(0);
	RoundGameState->SetTeamRoles(StartingTeamARole, StartingTeamBRole);
	bSidesSwitched = false;
	RoundGameState->ClearRoundResult();
	RoundGameState->SetMatchWinner(EGameTeam::None);
	RoundGameState->ClearRoundTimer();
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);

	EvaluateRoundStart();
}

void ABaseRoundGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!DefaultPawnClass)
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s has no DefaultPawnClass. Assign a project Character Blueprint in the GameMode defaults."), *GetNameSafe(this));
	}
	else if (DefaultPawnClass->IsChildOf(ADefaultPawn::StaticClass()))
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s still uses the engine DefaultPawn. Assign a project Character Blueprint to remove the automatic sphere pawn."), *GetNameSafe(this));
	}

	if (!PlayerControllerClass || !PlayerControllerClass->IsChildOf(ABaseRoundPlayerController::StaticClass()))
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s requires a PlayerController derived from ABaseRoundPlayerController."), *GetNameSafe(this));
	}

	if (!GameStateClass || !GameStateClass->IsChildOf(ABaseRoundGameState::StaticClass()))
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s requires a GameState derived from ABaseRoundGameState."), *GetNameSafe(this));
	}

	if (!PlayerStateClass || !PlayerStateClass->IsChildOf(ABaseTeamPlayerState::StaticClass()))
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s requires a PlayerState derived from ABaseTeamPlayerState."), *GetNameSafe(this));
	}

	if (!IsGameFlowConfigurationValid())
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s has an invalid round game-flow configuration."), *GetNameSafe(this));
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (IsValid(RoundGameState))
	{
		RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);
	}

	EvaluateRoundStart();
}

void ABaseRoundGameMode::InitGameState()
{
	Super::InitGameState();

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	RoundGameState->InitializeRoundConfiguration(MaxRounds);
	RoundGameState->SetTeamRoles(StartingTeamARole, StartingTeamBRole);
}

void ABaseRoundGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!IsValid(NewPlayer))
	{
		return;
	}

	EnsurePlayerTeamAssigned(NewPlayer);

	if (ABaseTeamPlayerState* TeamPlayerState = NewPlayer->GetPlayerState<ABaseTeamPlayerState>())
	{
		TeamPlayerState->SetAlive(false);
	}

	if (APawn* ExistingPawn = NewPlayer->GetPawn())
	{
		NewPlayer->UnPossess();
		ExistingPawn->Destroy();
	}

	if (ABaseRoundPlayerController* RoundPlayerController = Cast<ABaseRoundPlayerController>(NewPlayer))
	{
		RoundPlayerController->SetRoundInputEnabled(false);
	}
	else
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s is not derived from ABaseRoundPlayerController."), *GetNameSafe(NewPlayer));
	}

	EvaluateRoundStart();
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

void ABaseRoundGameMode::HandleGameFlowReset()
{
}

bool ABaseRoundGameMode::PrepareRound()
{
	return true;
}

void ABaseRoundGameMode::HandleRoundStarted()
{
}

void ABaseRoundGameMode::HandleRoundEnded(const EGameTeam, const ERoundEndReason)
{
}

void ABaseRoundGameMode::HandlePlayerEliminated(ABaseTeamPlayerState*)
{
}

void ABaseRoundGameMode::HandleRoundTimeExpired()
{
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

	GetWorldTimerManager().ClearTimer(RoundTimeTimerHandle);
	RoundGameState->ClearRoundTimer();
	RoundGameState->AddTeamScore(WinningTeam);
	RoundGameState->SetRoundResult(WinningTeam, EndReason);
	SetRoundPlayerInputEnabled(false);
	HandleRoundEnded(WinningTeam, EndReason);

	EGameTeam MatchWinningTeam = EGameTeam::None;
	if (IsMatchComplete(MatchWinningTeam))
	{
		FinishMatch(MatchWinningTeam);
		return;
	}

	RoundGameState->SetGameFlowPhase(EGameFlowPhase::RoundEnding, RoundEndDelay);
	if (RoundEndDelay <= 0.0f)
	{
		HandleRoundEndDelayFinished();
		return;
	}

	GetWorldTimerManager().SetTimer(RoundEndTimerHandle, this, &ABaseRoundGameMode::HandleRoundEndDelayFinished, RoundEndDelay, false);
}

void ABaseRoundGameMode::SetRoundTimer(const float Duration)
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RoundTimeTimerHandle);

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	RoundGameState->SetRoundTimer(Duration);
	if (Duration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RoundTimeTimerHandle, this, &ABaseRoundGameMode::HandleRoundTimeExpired, Duration, false);
	}
}

void ABaseRoundGameMode::FreezeRoundTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RoundTimeTimerHandle);

	if (ABaseRoundGameState* RoundGameState = GetBaseRoundGameState())
	{
		RoundGameState->FreezeRoundTimer();
	}
}

bool ABaseRoundGameMode::PreparePlayersForRound()
{
	TArray<APlayerController*> RoundPlayers;
	bool bAllPlayersReady = true;

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (!IsValid(PlayerController))
		{
			continue;
		}

		ABaseTeamPlayerState* TeamPlayerState = PlayerController->GetPlayerState<ABaseTeamPlayerState>();
		if (IsValid(TeamPlayerState))
		{
			TeamPlayerState->SetAlive(false);
		}

		if (APawn* ExistingPawn = PlayerController->GetPawn())
		{
			PlayerController->UnPossess();
			ExistingPawn->Destroy();
		}

		if (!IsValid(TeamPlayerState) || TeamPlayerState->GetTeam() == EGameTeam::None)
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s cannot enter the round because its team is not assigned."), *GetNameSafe(PlayerController));
			bAllPlayersReady = false;
			continue;
		}

		RoundPlayers.Add(PlayerController);
	}

	if (!bAllPlayersReady)
	{
		return false;
	}

	for (APlayerController* PlayerController : RoundPlayers)
	{
		RestartPlayer(PlayerController);

		ABaseTeamPlayerState* TeamPlayerState = PlayerController->GetPlayerState<ABaseTeamPlayerState>();
		const bool bSpawnedSuccessfully = IsValid(PlayerController->GetPawn());
		if (IsValid(TeamPlayerState))
		{
			TeamPlayerState->SetAlive(bSpawnedSuccessfully);
		}

		if (!bSpawnedSuccessfully)
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("Failed to spawn a round pawn for %s. Check DefaultPawnClass and PlayerStart configuration."), *GetNameSafe(PlayerController));
			bAllPlayersReady = false;
		}
	}

	return bAllPlayersReady;
}

ABaseRoundGameState* ABaseRoundGameMode::GetBaseRoundGameState() const
{
	return GetGameState<ABaseRoundGameState>();
}

EGameTeam ABaseRoundGameMode::GetTeamForRole(const ETeamRole TeamRole) const
{
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	return IsValid(RoundGameState) ? RoundGameState->GetTeamForRole(TeamRole) : EGameTeam::None;
}

void ABaseRoundGameMode::EvaluateRoundStart()
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

	if (!HasEnoughPlayersToStart())
	{
		if (RoundGameState->GetGameFlowPhase() == EGameFlowPhase::RoundStarting)
		{
			GetWorldTimerManager().ClearTimer(RoundStartTimerHandle);
			RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);
		}
		return;
	}

	if (RoundGameState->GetGameFlowPhase() == EGameFlowPhase::WaitingForPlayers)
	{
		ScheduleRoundStart(InitialRoundStartDelay);
	}
}

void ABaseRoundGameMode::ScheduleRoundStart(const float Delay)
{
	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	RoundGameState->SetGameFlowPhase(EGameFlowPhase::RoundStarting, Delay);

	if (Delay <= 0.0f)
	{
		BeginNextRound();
		return;
	}

	GetWorldTimerManager().SetTimer(RoundStartTimerHandle, this, &ABaseRoundGameMode::BeginNextRound, Delay, false);
}

void ABaseRoundGameMode::BeginNextRound()
{
	if (!HasEnoughPlayersToStart())
	{
		SetRoundPlayerInputEnabled(false);
		ResetAllPlayerPawns();

		if (ABaseRoundGameState* RoundGameState = GetBaseRoundGameState())
		{
			RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);
		}
		return;
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	const int32 NextRoundNumber = RoundGameState->GetCurrentRoundNumber() + 1;
	if (SideSwitchRound > 0 && NextRoundNumber == SideSwitchRound && !bSidesSwitched)
	{
		RoundGameState->SetTeamRoles(RoundGameState->GetTeamRole(EGameTeam::TeamB), RoundGameState->GetTeamRole(EGameTeam::TeamA));
		bSidesSwitched = true;
	}

	RoundGameState->ClearRoundResult();
	if (!PreparePlayersForRound() || !PrepareRound())
	{
		SetRoundPlayerInputEnabled(false);
		ResetAllPlayerPawns();
		RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);
		return;
	}

	if (!HasMatchStarted())
	{
		StartMatch();
	}

	RoundGameState->SetCurrentRoundNumber(NextRoundNumber);
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::RoundInProgress);
	SetRoundTimer(RoundDuration);
	HandleRoundStarted();

	if (RoundGameState->GetGameFlowPhase() == EGameFlowPhase::RoundInProgress)
	{
		SetRoundPlayerInputEnabled(true);
	}
}

void ABaseRoundGameMode::HandleRoundEndDelayFinished()
{
	if (!HasEnoughPlayersToStart())
	{
		SetRoundPlayerInputEnabled(false);
		ResetAllPlayerPawns();

		if (ABaseRoundGameState* RoundGameState = GetBaseRoundGameState())
		{
			RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);
		}
		return;
	}

	ScheduleRoundStart(RoundStartDelay);
}

void ABaseRoundGameMode::FinishMatch(const EGameTeam WinningTeam)
{
	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RoundStartTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundTimeTimerHandle);
	SetRoundPlayerInputEnabled(false);

	RoundGameState->ClearRoundTimer();
	RoundGameState->SetMatchWinner(WinningTeam);
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::MatchEnded);
	EndMatch();
}

void ABaseRoundGameMode::ResetAllPlayerPawns()
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (!IsValid(PlayerController))
		{
			continue;
		}

		if (ABaseTeamPlayerState* TeamPlayerState = PlayerController->GetPlayerState<ABaseTeamPlayerState>())
		{
			TeamPlayerState->SetAlive(false);
		}

		if (APawn* ExistingPawn = PlayerController->GetPawn())
		{
			PlayerController->UnPossess();
			ExistingPawn->Destroy();
		}
	}
}

void ABaseRoundGameMode::SetRoundPlayerInputEnabled(const bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (!IsValid(PlayerController))
		{
			continue;
		}

		ABaseRoundPlayerController* RoundPlayerController = Cast<ABaseRoundPlayerController>(PlayerController);
		if (!IsValid(RoundPlayerController))
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s is not derived from ABaseRoundPlayerController; round input state cannot be synchronized."), *GetNameSafe(PlayerController));
			continue;
		}

		RoundPlayerController->SetRoundInputEnabled(bEnabled);
	}
}

bool ABaseRoundGameMode::IsGameFlowConfigurationValid() const
{
	return DefaultPawnClass != nullptr
		&& !DefaultPawnClass->IsChildOf(ADefaultPawn::StaticClass())
		&& PlayerControllerClass != nullptr
		&& PlayerControllerClass->IsChildOf(ABaseRoundPlayerController::StaticClass())
		&& GameStateClass != nullptr
		&& GameStateClass->IsChildOf(ABaseRoundGameState::StaticClass())
		&& PlayerStateClass != nullptr
		&& PlayerStateClass->IsChildOf(ABaseTeamPlayerState::StaticClass())
		&& SpawnClearanceRadius >= 0.0f
		&& MinimumPlayersToStart > 0
		&& InitialRoundStartDelay >= 0.0f
		&& RoundStartDelay >= 0.0f
		&& RoundEndDelay >= 0.0f
		&& RoundDuration > 0.0f
		&& SideSwitchRound >= 0
		&& RoundsToWin >= 0
		&& MaxRounds >= 0;
}

bool ABaseRoundGameMode::HasEnoughPlayersToStart() const
{
	if (!IsGameFlowConfigurationValid())
	{
		return false;
	}

	const int32 AssignedPlayerCount = GetTeamPlayerCount(EGameTeam::TeamA) + GetTeamPlayerCount(EGameTeam::TeamB);
	return AssignedPlayerCount >= MinimumPlayersToStart;
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

	if (MaxRounds > 0 && RoundGameState->GetCurrentRoundNumber() >= MaxRounds)
	{
		OutWinningTeam = RoundGameState->GetLeadingTeam();
		return true;
	}

	return false;
}
