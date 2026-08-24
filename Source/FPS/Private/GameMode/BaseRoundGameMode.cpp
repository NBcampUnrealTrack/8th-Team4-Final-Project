#include "GameMode/BaseRoundGameMode.h"

#include "Economy/CreditEconomyManager.h"
#include "GameMode/AugmentDraftManager.h"
#include "BaseRoundGameModeLog.h"
#include "GameMode/BaseRoundGameState.h"
#include "GameMode/SurrenderVoteManager.h"
#include "GameMode/TeamPlayerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/BaseRoundPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Shop/ShopComponent.h"
#include "Shop/ShopPurchaseComponent.h"
#include "Weapon/WeaponInventoryComponent.h"
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
	SurrenderVoteManager = CreateDefaultSubobject<USurrenderVoteManager>(TEXT("SurrenderVoteManager"));
}

void ABaseRoundGameMode::Logout(AController* Exiting)
{
	InitialPlayerReadinessTracker.RemovePlayer(Cast<APlayerController>(Exiting));

	ABaseRoundPlayerController* ExitingPlayerController = Cast<ABaseRoundPlayerController>(Exiting);
	if (IsValid(ExitingPlayerController))
	{
		GetSurrenderVoteManager()->HandleVoterLeaving(ExitingPlayerController);
	}

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	const EGameFlowPhase Phase = IsValid(RoundGameState)
		? RoundGameState->GetGameFlowPhase()
		: EGameFlowPhase::WaitingForPlayers;
	AShooterPlayerState* ExitingPlayerState = IsValid(Exiting)
		? Exiting->GetPlayerState<AShooterPlayerState>()
		: nullptr;

	// Super::Logout 이전에는 PlayerState와 Wallet이 유효하므로 미확정 거래를 정상 환불할 수 있다.
	if (IsValid(ExitingPlayerState))
	{
		RefundPendingShopPurchasesForPlayer(ExitingPlayerState, TEXT("logout"));
	}

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

bool ABaseRoundGameMode::GatherRoundParticipantPurchaseComponents(
	TArray<UShopPurchaseComponent*>& OutPurchaseComponents) const
{
	OutPurchaseComponents.Reset();

	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	if (!IsValid(PlayerManager))
	{
		return false;
	}

	TArray<AShooterPlayerState*> Participants;
	PlayerManager->GetRoundParticipants(Participants);
	OutPurchaseComponents.Reserve(Participants.Num());

	for (AShooterPlayerState* PlayerState : Participants)
	{
		if (!IsValid(PlayerState))
		{
			return false;
		}

		UShopPurchaseComponent* PurchaseComponent = PlayerState->GetShopPurchaseComponent();

		if (!IsValid(PurchaseComponent))
		{
			UE_LOG(
				LogBaseRoundGameMode,
				Error,
				TEXT("Cannot commit shop purchases for %s: ShopPurchaseComponent is missing."),
				*GetNameSafe(PlayerState));

			return false;
		}

		OutPurchaseComponents.Add(PurchaseComponent);
	}

	return true;
}

bool ABaseRoundGameMode::CanCommitPendingShopPurchases() const
{
	TArray<UShopPurchaseComponent*> PurchaseComponents;
	if (!GatherRoundParticipantPurchaseComponents(PurchaseComponents))
	{
		return false;
	}

	for (const UShopPurchaseComponent* PurchaseComponent : PurchaseComponents)
	{
		if (!PurchaseComponent->CanCommitPendingPurchases())
		{
			UE_LOG(
				LogBaseRoundGameMode,
				Error,
				TEXT("Cannot commit pending shop purchases for %s."),
				*GetNameSafe(PurchaseComponent->GetOwner()));

			return false;
		}
	}

	return true;
}

bool ABaseRoundGameMode::CommitPendingShopPurchases()
{
	TArray<UShopPurchaseComponent*> PurchaseComponents;
	if (!GatherRoundParticipantPurchaseComponents(PurchaseComponents))
	{
		return false;
	}

	for (UShopPurchaseComponent* PurchaseComponent : PurchaseComponents)
	{
		if (!PurchaseComponent->CommitPendingPurchases())
		{
			UE_LOG(
				LogBaseRoundGameMode,
				Error,
				TEXT("Failed to commit prevalidated shop purchases for %s."),
				*GetNameSafe(PurchaseComponent->GetOwner()));

			return false;
		}
	}

	return true;
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
	if (USurrenderVoteManager* VoteManager = GetSurrenderVoteManager())
	{
		VoteManager->ResetForNewMatch();
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
	if (USurrenderVoteManager* VoteManager = GetSurrenderVoteManager())
	{
		VoteManager->Shutdown();
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

	// 의도적으로 Super를 호출하지 않는다. 라운드 모드는 입장 시 즉시 스폰하지 않고,
	// 팀 초기화 후 다음 라운드 준비 단계에서 참가자를 한 번에 스폰한다.
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

	if (USurrenderVoteManager* VoteManager = GetSurrenderVoteManager())
	{
		VoteManager->CancelAllActiveVotes();
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
	TArray<UShopComponent*> MatchShops;
	MatchPlayers.Reserve(RoundGameState->PlayerArray.Num());
	MatchShops.Reserve(RoundGameState->PlayerArray.Num());

	// 1단계: 아무것도 바꾸지 않고 상점과 경제를 모두 검증한다.
	// 거래 파기는 되돌릴 수 없으므로 실패 가능성이 남아 있는 동안에는 실행하지 않는다.
	for (APlayerState* PlayerState : RoundGameState->PlayerArray)
	{
		AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (!IsValid(ShooterPlayerState))
		{
			continue;
		}

		UShopComponent* Shop = ShooterPlayerState->GetShopComponent();
		FString ShopFailureReason;
		if (!IsValid(Shop) || !Shop->CanDiscardAllActiveTransactionsForNewMatch(&ShopFailureReason))
		{
			UE_LOG(LogBaseRoundGameMode, Error,
				TEXT("Cannot reset economy because shop transactions could not be closed for %s: %s."),
				*GetNameSafe(ShooterPlayerState),
				IsValid(Shop) ? *ShopFailureReason : TEXT("the shop component is missing"));
			return false;
		}

		MatchPlayers.Add(ShooterPlayerState);
		MatchShops.Add(Shop);
	}

	FString EconomyFailureReason;
	if (!Economy->CanResetForNewMatch(MatchPlayers, &EconomyFailureReason))
	{
		UE_LOG(LogBaseRoundGameMode, Error,
			TEXT("Cannot reset economy for a new match: %s."),
			*EconomyFailureReason);
		return false;
	}

	// 2단계: 검증이 끝났으므로 거래를 닫고 Wallet을 초기화한다.
	// 사전 검증을 통과했으므로 여기서 중단하면 일부만 파기된 채 Wallet 초기화를 놓친다.
	// 실패는 버그로 보고, 나머지 상점의 파기와 Wallet 초기화는 끝까지 진행한다.
	bool bAllTransactionsDiscarded = true;

	for (UShopComponent* Shop : MatchShops)
	{
		if (!ensureMsgf(
			IsValid(Shop) && Shop->DiscardAllActiveTransactionsForNewMatch(),
			TEXT("Shop transaction discard failed for %s after validation succeeded."),
			IsValid(Shop) ? *GetNameSafe(Shop->GetOwner()) : TEXT("None")))
		{
			bAllTransactionsDiscarded = false;
			continue;
		}
	}

	for (AShooterPlayerState* MatchPlayer : MatchPlayers)
	{
		if (UWeaponInventoryComponent* Inventory = MatchPlayer->GetWeaponInventory())
		{
			Inventory->ServerInitializeForNewMatch();
		}
	}

	const bool bEconomyReset = Economy->ResetForNewMatch(MatchPlayers);
	return bEconomyReset && bAllTransactionsDiscarded;
}

void ABaseRoundGameMode::RefundPendingShopPurchasesForPlayer(
	AShooterPlayerState* PlayerState,
	const TCHAR* Context)
{
	if (!IsValid(PlayerState))
	{
		return;
	}

	const TCHAR* CleanupContext = Context ? Context : TEXT("unspecified cleanup");

	UShopPurchaseComponent* PurchaseComponent =	PlayerState->GetShopPurchaseComponent();

	if (!IsValid(PurchaseComponent))
	{
		UE_LOG(
			LogBaseRoundGameMode,
			Error,
			TEXT("Cannot refund pending shop purchases for %s during %s: ShopPurchaseComponent is missing."),
			*GetNameSafe(PlayerState),
			CleanupContext);

		return;
	}

	if (!PurchaseComponent->CancelPendingPurchases())
	{
		UE_LOG(
			LogBaseRoundGameMode,
			Error,
			TEXT("Failed to cancel pending shop purchases for %s during %s."),
			*GetNameSafe(PlayerState),
			CleanupContext);

		return;
	}

	UE_LOG(
		LogBaseRoundGameMode,
		Verbose,
		TEXT("Pending shop purchases cleaned up for %s during %s."),
		*GetNameSafe(PlayerState),
		CleanupContext);
}

void ABaseRoundGameMode::RefundPendingShopPurchases(const TCHAR* Context)
{
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : RoundGameState->PlayerArray)
	{
		AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);

		if (IsValid(ShooterPlayerState))
		{
			RefundPendingShopPurchasesForPlayer(ShooterPlayerState,	Context);
		}
	}
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

USurrenderVoteManager* ABaseRoundGameMode::GetSurrenderVoteManager() const
{
	return SurrenderVoteManager.Get();
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
	if (USurrenderVoteManager* VoteManager = GetSurrenderVoteManager())
	{
		VoteManager->Initialize(this, SurrenderVoteDuration, SurrenderVoteCooldown);
	}

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
