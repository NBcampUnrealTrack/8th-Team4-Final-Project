#include "GameMode/BaseRoundGameMode.h"

#include "Economy/CreditEconomyManager.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "BaseRoundGameModeLog.h"
#include "GameMode/State/BaseRoundGameState.h"
#include "GameMode/Manager/TeamPlayerManager.h"
#include "Player/BaseRoundPlayerController.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "TimerManager.h"

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
	return InitialPlayerReadinessTracker
		.CaptureSnapshot(GetWorld())
		.AreAllAssignedPlayersReady(MinimumPlayersToStart);
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

	const FInitialPlayerReadinessSnapshot ReadinessSnapshot =
		InitialPlayerReadinessTracker.CaptureSnapshot(GetWorld());
	for (const TWeakObjectPtr<APlayerController>& PlayerControllerPtr
		: ReadinessSnapshot.AssignedPlayerControllers)
	{
		APlayerController* PlayerController = PlayerControllerPtr.Get();
		ABaseTeamPlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ABaseTeamPlayerState>()
			: nullptr;
		if (!IsValid(PlayerState))
		{
			continue;
		}

		const bool bIsReady = InitialPlayerReadinessTracker.IsPlayerReady(PlayerController);

		if (AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState))
		{
			ShooterPlayerState->SetLoadedIntoMatch(bIsReady);
		}
	}

	RoundGameState->SetLoadingStatus(
		ReadinessSnapshot.ReadyPlayerCount,
		ReadinessSnapshot.AssignedPlayerCount);
}

bool ABaseRoundGameMode::IsBaseConfigurationValid() const
{
	const bool bRolesAreValid = StartingTeamARole != ETeamRole::None
		&& StartingTeamBRole != ETeamRole::None
		&& StartingTeamARole != StartingTeamBRole;
	const UCreditEconomyManager* Economy = GetCreditEconomyManager();

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
		&& IsSurrenderConfigurationValid()
		&& IsValid(Economy)
		&& Economy->IsConfigurationValid()
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
