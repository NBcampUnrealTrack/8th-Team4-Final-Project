#include "GameMode/BaseRoundGameMode.h"

#include "Economy/CreditEconomyManager.h"
#include "Engine/GameInstance.h"
#include "GameMode/AugmentDraftManager.h"
#include "BaseRoundGameModeLog.h"
#include "GameMode/BaseRoundGameState.h"
#include "GameMode/TeamPlayerManager.h"
#include "Player/ShooterPlayerState.h"
#include "Session/SessionSubSystem.h"
#include "TimerManager.h"

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
		&& !InitialPlayerReadinessTracker.HasTimeoutExpired()
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

	if (!InitializeRoundParticipantCredits())
	{
		SuspendRoundOpening(TEXT("A round participant could not be initialized for the credit economy."));
		return;
	}

	if (!PrepareModeRound(RoundContext))
	{
		SuspendRoundOpening(TEXT("Mode-specific round preparation failed."));
		return;
	}

	UAugmentDraftManager* DraftManager = nullptr;
	if (bEnableAugmentDraft)
	{
		DraftManager = GetAugmentDraftManager();
		if (!IsValid(DraftManager))
		{
			SuspendRoundOpening(TEXT("The augment draft manager is missing."));
			return;
		}
	}

	// 모든 라운드 리소스가 준비된 뒤에만 새 라운드 정보를 공개한다.
	RoundGameState->SetRoundContext(RoundContext);
	if (bEnableAugmentDraft)
	{
		// 서버 페이즈를 먼저 전환한 뒤 후보를 제시
		RoundGameState->SetGameFlowPhase(
			EGameFlowPhase::AugmentSelecting,
			AugmentSelectionDuration);

		// 모든 플레이어에게 후보 등록, 폭탄 모드는 모든 증강 풀 적용
		DraftManager->GrantToAll(EAugmentRewardPool::Any, RoundContext.RoundNumber);

		// 등록된 후보를 모든 참가자에게 제공
		TArray<AShooterPlayerState*> Participants;
		PlayerManager->GetRoundParticipants(Participants);
		for (AShooterPlayerState* Participant : Participants)
		{
			DraftManager->ServeNextReward(Participant);
		}

		// 증강 선택 페이즈에도 참가자의 이동과 액션 입력을 허용한다.
		// 베이스 이탈은 맵에 배치한 AugmentPhaseBarrier가 제한한다.
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
		SuspendRoundOpening(TEXT("Round combat transition failed."));
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

	// 라운드에 참여하는 모든 플레이어의 Pending 거래를 먼저 확인한다.
	// 이 검사 자체는 상태를 바꾸지 않으며, 실패 정리는 기존 SuspendRoundOpening 흐름이 담당한다.
	if (!CanCommitPendingShopPurchases())
	{
		UE_LOG(
			LogBaseRoundGameMode,
			Error,
			TEXT("Cannot start round combat because shop purchases are not ready to commit."));
		return false;
	}

	// Draft 태그가 먼저 복제돼도 전투 페이즈 공개 전에는 입력이 열리지 않게 한다.
	SetRoundParticipantInputEnabled(false);

	UCreditEconomyManager* Economy = GetCreditEconomyManager();
	const int32 RoundNumber = RoundGameState->GetCurrentRoundNumber();
	if (!IsValid(Economy) || !Economy->BeginRound(RoundNumber))
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("Credit economy could not begin round %d."), RoundNumber);
		return false;
	}

	// Economy가 열린 뒤 모드와 드래프트의 되돌리기 어려운 작업을 확정한다.
	if (!ActivateModeRound())
	{
		return false;
	}

	if (bEnableAugmentDraft)
	{
		UAugmentDraftManager* DraftManager = GetAugmentDraftManager();
		if (!IsValid(DraftManager))
		{
			return false;
		}

		// 선택하지 않은 플레이어는 강제로 선택
		TArray<AShooterPlayerState*> Participants;
		if (IsValid(PlayerManager))
		{
			PlayerManager->GetRoundParticipants(Participants);
		}

		for (AShooterPlayerState* Participant : Participants)
		{
			DraftManager->ForceResolveOffers(Participant);
		}
	}

	if (!CommitPendingShopPurchases())
	{
		UE_LOG(
			LogBaseRoundGameMode,
			Error,
			TEXT("Cannot start round combat because shop purchases could not be committed."));

		return false;
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

	// 다음 매치의 ResetEconomyForNewMatch가 어차피 파기하지만, 결과 화면에서
	// 잔액이 차감된 채로 보이지 않도록 매치 종료 시점에 원장을 비운다.
	RefundPendingShopPurchases(TEXT("match end"));

	if (UAugmentDraftManager* DraftManager = GetAugmentDraftManager())
	{
		DraftManager->CancelDraft();
	}
	if (UCreditEconomyManager* Economy = GetCreditEconomyManager())
	{
		Economy->CancelActiveRound();
	}
	RoundGameState->SetMatchWinner(WinningTeam);
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::MatchEnded);
	EndMatch();

	if (RoundEndDelay <= 0.0f)
	{
		ReturnToLobbyAfterMatch();
		return;
	}

	GetWorldTimerManager().SetTimer(
		PhaseTimerHandle,
		this,
		&ThisClass::ReturnToLobbyAfterMatch,
		RoundEndDelay,
		false);
}

void ABaseRoundGameMode::ReturnToLobbyAfterMatch()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USessionSubSystem* SessionSubsystem = GameInstance->GetSubsystem<USessionSubSystem>())
		{
			if (SessionSubsystem->ReturnRoomToLobby())
			{
				return;
			}
		}
	}

	// 세션 계층을 사용할 수 없는 예외 상황에서는 기존 안전 경로로 빠진다.
	ReturnToMainMenuHost();
}

void ABaseRoundGameMode::EnterWaitingForPlayers()
{
	ClearAllFlowTimers();
	SetAllPlayersRoundInputEnabled(false);
	SpawnPreparationRetryCount = 0;

	// 상점 창은 AugmentSelecting에서 열린다. 라운드가 되감기면 같은 매치이므로
	// Wallet 초기화가 없다. 미확정 결제를 여기서 되돌려 원장을 비운다.
	RefundPendingShopPurchases(TEXT("round rollback"));

	if (UCreditEconomyManager* Economy = GetCreditEconomyManager())
	{
		Economy->CancelActiveRound();
	}

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

	const FInitialPlayerReadinessSnapshot ReadinessSnapshot =
		InitialPlayerReadinessTracker.CaptureSnapshot(GetWorld());
	const FString PendingPlayerList = ReadinessSnapshot.UnreadyPlayerNames.IsEmpty()
		? TEXT("<unknown>")
		: FString::Join(ReadinessSnapshot.UnreadyPlayerNames, TEXT(", "));
	UE_LOG(LogBaseRoundGameMode, Warning,
		TEXT("Initial player ready gate timed out after %.1f seconds. Continuing while UI initialization keeps retrying. Unready players: %s"),
		InitialReadyTimeout,
		*PendingPlayerList);
	InitialPlayerReadinessTracker.MarkTimeoutExpired();
	TryBeginNextRound();
}

bool ABaseRoundGameMode::IsInitialJoinPhase(const EGameFlowPhase Phase) const
{
	return LastCompletedRoundNumber == 0
		&& (Phase == EGameFlowPhase::WaitingForPlayers
			|| Phase == EGameFlowPhase::WaitingForPlayersReady);
}

EPlayerParticipationState ABaseRoundGameMode::ResolveParticipationStateForNewPlayer(APlayerController*) const
{
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	const EGameFlowPhase Phase = IsValid(RoundGameState)
		? RoundGameState->GetGameFlowPhase()
		: EGameFlowPhase::WaitingForPlayers;

	if (IsInitialJoinPhase(Phase))
	{
		return EPlayerParticipationState::WaitingForMatchStart;
	}

	return EPlayerParticipationState::WaitingForNextSpawn;
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
