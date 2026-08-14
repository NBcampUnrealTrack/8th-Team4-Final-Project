#include "GameFlow/Round/BaseRoundGameMode.h"

#include "Augment/AugmentInventoryComponent.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Augment/AugmentTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFlow/Common/BaseTeamPlayerState.h"
#include "GameFlow/Round/BaseRoundGameState.h"
#include "GameFlow/Round/BaseRoundPlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
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
	if (!IsValid(EliminatedPlayerState))
	{
		return;
	}

	const TWeakObjectPtr<ABaseTeamPlayerState> PlayerKey(EliminatedPlayerState);
	if (EliminatedPlayersThisRound.Contains(PlayerKey))
	{
		return;
	}

	EliminatedPlayersThisRound.Add(PlayerKey);
	EliminatedPlayerState->SetAlive(false);

	// 2v2처럼 라운드가 계속되는 경우 사망한 플레이어만 즉시 입력을 잠근다.
	// 다음 라운드의 StartRoundCombat()에서 전 플레이어 입력을 다시 활성화한다.
	if (ABaseRoundPlayerController* RoundPlayerController = Cast<ABaseRoundPlayerController>(EliminatedController))
	{
		RoundPlayerController->SetRoundInputEnabled(false);
	}

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
	GetWorldTimerManager().ClearTimer(AugmentSelectionTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundTimeTimerHandle);
	SetRoundPlayerInputEnabled(false);
	EliminatedPlayersThisRound.Reset();
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
	bLoggedWaitingForAugmentRegistry = false;
	RoundGameState->ClearRoundResult();
	RoundGameState->SetMatchWinner(EGameTeam::None);
	RoundGameState->ClearRoundTimer();
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);

	EvaluateRoundStart();
}

void ABaseRoundGameMode::NotifyPlayerLoadedIntoMatch(APlayerController* PlayerController)
{
	if (!HasAuthority())
	{
		return;
	}

	AShooterPlayerState* ShooterPlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	if (IsValid(ShooterPlayerState))
	{
		ShooterPlayerState->SetLoadedIntoMatch(true);
	}

	UpdateMatchLoadingStatus();
	EvaluateRoundStart();
}

void ABaseRoundGameMode::BeginPlay()
{
	Super::BeginPlay();

	RegenerateMatchSeed();

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

	if (!PlayerStateClass || !PlayerStateClass->IsChildOf(AShooterPlayerState::StaticClass()))
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s requires AShooterPlayerState (or a derived class) for round/GAS integration."), *GetNameSafe(this));
	}

	if (!IsGameFlowConfigurationValid())
	{
		UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s has an invalid round game-flow configuration."), *GetNameSafe(this));
	}

	if (bEnableAugmentDraft)
	{
		UAugmentRegistrySubsystem* Registry = nullptr;
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				Registry = GameInstance->GetSubsystem<UAugmentRegistrySubsystem>();
			}
		}

		if (IsValid(Registry))
		{
			BoundAugmentRegistry = Registry;
			if (!Registry->IsReady())
			{
				Registry->OnRegistryReady.AddUniqueDynamic(this, &ABaseRoundGameMode::HandleAugmentRegistryReady);
			}
		}
		else
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s cannot find UAugmentRegistrySubsystem."), *GetNameSafe(this));
		}
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (IsValid(RoundGameState))
	{
		RoundGameState->ClearLoadingStatus();
		RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);
	}

	EvaluateRoundStart();
}

void ABaseRoundGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RoundStartTimerHandle);
	GetWorldTimerManager().ClearTimer(AugmentSelectionTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundTimeTimerHandle);

	if (UAugmentRegistrySubsystem* Registry = BoundAugmentRegistry.Get())
	{
		Registry->OnRegistryReady.RemoveDynamic(this, &ABaseRoundGameMode::HandleAugmentRegistryReady);
	}
	BoundAugmentRegistry.Reset();

	Super::EndPlay(EndPlayReason);
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

	if (AShooterPlayerState* ShooterPlayerState = NewPlayer->GetPlayerState<AShooterPlayerState>())
	{
		ShooterPlayerState->SetLoadedIntoMatch(false);

		// 새 매치의 시작 경계에서는 PlayerState가 seamless travel로 재사용되더라도
		// 이전 매치의 증강/미선택 후보가 남지 않도록 GAS 쪽 공개 초기화 API를 사용한다.
		const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
		if (IsValid(RoundGameState)
			&& RoundGameState->GetCurrentRoundNumber() == 0
			&& RoundGameState->GetGameFlowPhase() == EGameFlowPhase::WaitingForPlayers)
		{
			if (UAugmentInventoryComponent* Inventory = ShooterPlayerState->GetAugmentInventory())
			{
				Inventory->ServerRemoveAllAugments();
			}
		}

		ShooterPlayerState->SetAlive(false);
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

	UpdateMatchLoadingStatus();
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
	ResetAllPlayerAugments();
	RegenerateMatchSeed();
}

bool ABaseRoundGameMode::PrepareRound()
{
	return true;
}

void ABaseRoundGameMode::HandlePreparedRoundAborted()
{
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
	EliminatedPlayersThisRound.Reset();

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

		const bool bSpawnedSuccessfully = IsValid(PlayerController->GetPawn());
		AShooterPlayerState* ShooterPlayerState = PlayerController->GetPlayerState<AShooterPlayerState>();
		if (!bSpawnedSuccessfully || !IsValid(ShooterPlayerState))
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("Failed to prepare %s for the round. Check DefaultPawnClass, PlayerStart and AShooterPlayerState."), *GetNameSafe(PlayerController));
			bAllPlayersReady = false;
			continue;
		}

		ShooterPlayerState->ServerRespawnReset();
		if (!ShooterPlayerState->IsAlive())
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s remained dead after ServerRespawnReset()."), *GetNameSafe(ShooterPlayerState));
			bAllPlayersReady = false;
		}
	}

	return bAllPlayersReady;
}

bool ABaseRoundGameMode::AreAllPlayersLoadedIntoMatch() const
{
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || RoundGameState->PlayerArray.IsEmpty())
	{
		return false;
	}

	for (APlayerState* PlayerState : RoundGameState->PlayerArray)
	{
		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (!IsValid(ShooterPlayerState) || !ShooterPlayerState->IsLoadedIntoMatch())
		{
			return false;
		}
	}

	return true;
}

void ABaseRoundGameMode::UpdateMatchLoadingStatus()
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

	int32 LoadedCount = 0;
	int32 RequiredCount = 0;
	for (APlayerState* PlayerState : RoundGameState->PlayerArray)
	{
		if (const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState))
		{
			++RequiredCount;
			if (ShooterPlayerState->IsLoadedIntoMatch())
			{
				++LoadedCount;
			}
		}
	}

	RoundGameState->SetLoadingStatus(LoadedCount, RequiredCount);
}

void ABaseRoundGameMode::SetAllLoadingScreensVisible(const float Progress)
{
	if (!HasAuthority())
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (AShooterPlayerController* ShooterPlayerController = Cast<AShooterPlayerController>(Iterator->Get()))
		{
			ShooterPlayerController->ClientShowLoadingScreen(Progress);
		}
	}
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
		else if (RoundGameState->GetGameFlowPhase() == EGameFlowPhase::AugmentSelecting)
		{
			// 선택 완료자가 일부 생긴 뒤 인원이 빠지면 같은 라운드를 롤백해 재추첨할 경우
			// 증강을 중복 획득할 수 있다. 재접속 복구가 CUT인 프로토타입에서는 세션 흐름을
			// 초기화하는 편이 상태 일관성을 보장한다.
			RestartGameFlow();
		}
		return;
	}

	if (RoundGameState->GetGameFlowPhase() != EGameFlowPhase::WaitingForPlayers)
	{
		return;
	}

	if (!AreAllPlayersLoadedIntoMatch())
	{
		UpdateMatchLoadingStatus();
		SetAllLoadingScreensVisible(0.7f);
		return;
	}

	if (bEnableAugmentDraft && !IsAugmentRegistryReady())
	{
		if (!bLoggedWaitingForAugmentRegistry)
		{
			bLoggedWaitingForAugmentRegistry = true;
			UE_LOG(LogBaseRoundGameMode, Display, TEXT("Waiting for the augment registry before opening the first round."));
		}
		return;
	}

	bLoggedWaitingForAugmentRegistry = false;
	ScheduleRoundStart(InitialRoundStartDelay);
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
		AbortPreparedRound(false);
		return;
	}

	if (!HasMatchStarted())
	{
		StartMatch();
	}

	RoundGameState->SetCurrentRoundNumber(NextRoundNumber);

	if (bEnableAugmentDraft)
	{
		if (!RollAugmentOffersForPlayers(NextRoundNumber))
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("Round %d was not opened because augment offer generation failed."), NextRoundNumber);
			AbortPreparedRound(true);
			return;
		}

		BeginAugmentSelectionPhase();
		return;
	}

	StartRoundCombat();
}

void ABaseRoundGameMode::BeginAugmentSelectionPhase()
{
	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	SetRoundPlayerInputEnabled(false);
	RoundGameState->ClearRoundTimer();
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::AugmentSelecting, AugmentSelectionDuration);

	if (AugmentSelectionDuration <= 0.0f)
	{
		FinishAugmentSelectionPhase();
		return;
	}

	GetWorldTimerManager().SetTimer(
		AugmentSelectionTimerHandle,
		this,
		&ABaseRoundGameMode::FinishAugmentSelectionPhase,
		AugmentSelectionDuration,
		false);
}

void ABaseRoundGameMode::FinishAugmentSelectionPhase()
{
	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || RoundGameState->GetGameFlowPhase() != EGameFlowPhase::AugmentSelecting)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AugmentSelectionTimerHandle);

	if (!HasEnoughPlayersToStart())
	{
		RestartGameFlow();
		return;
	}

	if (!AutoSelectAugmentOffersForPlayers())
	{
		UE_LOG(LogBaseRoundGameMode, Error,
			TEXT("One or more players still have augment offers after auto selection. Combat will remain locked; check [AugmentInventory] errors."));
		return;
	}

	StartRoundCombat();
}

void ABaseRoundGameMode::StartRoundCombat()
{
	if (!HasEnoughPlayersToStart())
	{
		AbortPreparedRound(true);
		return;
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	RoundGameState->SetGameFlowPhase(EGameFlowPhase::RoundInProgress);
	SetRoundTimer(RoundDuration);
	HandleRoundStarted();
	UpdateMatchLoadingStatus();

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
	GetWorldTimerManager().ClearTimer(AugmentSelectionTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundTimeTimerHandle);
	SetRoundPlayerInputEnabled(false);

	RoundGameState->ClearRoundTimer();
	RoundGameState->SetMatchWinner(WinningTeam);
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::MatchEnded);
	EndMatch();
}

void ABaseRoundGameMode::AbortPreparedRound(const bool bRollbackRoundNumber)
{
	GetWorldTimerManager().ClearTimer(AugmentSelectionTimerHandle);
	SetRoundPlayerInputEnabled(false);
	HandlePreparedRoundAborted();
	ResetAllPlayerPawns();
	EliminatedPlayersThisRound.Reset();

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	if (bRollbackRoundNumber && RoundGameState->GetCurrentRoundNumber() > 0)
	{
		RoundGameState->SetCurrentRoundNumber(RoundGameState->GetCurrentRoundNumber() - 1);
	}

	RoundGameState->ClearRoundTimer();
	RoundGameState->SetGameFlowPhase(EGameFlowPhase::WaitingForPlayers);
	EvaluateRoundStart();
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

void ABaseRoundGameMode::ResetAllPlayerAugments()
{
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : RoundGameState->PlayerArray)
	{
		if (AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState))
		{
			if (UAugmentInventoryComponent* Inventory = ShooterPlayerState->GetAugmentInventory())
			{
				Inventory->ServerRemoveAllAugments();
			}
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

bool ABaseRoundGameMode::RollAugmentOffersForPlayers(const int32 RoundNumber)
{
	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || !IsAugmentRegistryReady())
	{
		return false;
	}

	const int32 RoundSeed = MakeRoundSeed(RoundNumber);
	bool bFoundPlayer = false;
	bool bAllRolled = true;

	for (APlayerState* PlayerState : RoundGameState->PlayerArray)
	{
		AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (!IsValid(ShooterPlayerState))
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("Round %d contains a PlayerState that is not AShooterPlayerState."), RoundNumber);
			bAllRolled = false;
			continue;
		}

		bFoundPlayer = true;
		UAugmentInventoryComponent* Inventory = ShooterPlayerState->GetAugmentInventory();
		if (!IsValid(Inventory))
		{
			UE_LOG(LogBaseRoundGameMode, Error, TEXT("%s has no AugmentInventoryComponent."), *GetNameSafe(ShooterPlayerState));
			bAllRolled = false;
			continue;
		}

		FAugmentDraftContext DraftContext;
		DraftContext.RoundNumber = RoundNumber;
		DraftContext.RoundSeed = RoundSeed;

		const EGameTeam PlayerTeam = ShooterPlayerState->GetTeam();
		if (PlayerTeam == EGameTeam::TeamA)
		{
			DraftContext.TeamScore = RoundGameState->GetTeamAScore();
			DraftContext.OpponentScore = RoundGameState->GetTeamBScore();
		}
		else if (PlayerTeam == EGameTeam::TeamB)
		{
			DraftContext.TeamScore = RoundGameState->GetTeamBScore();
			DraftContext.OpponentScore = RoundGameState->GetTeamAScore();
		}

		if (!Inventory->ServerRollOffers(DraftContext, AugmentOfferCount))
		{
			bAllRolled = false;
			continue;
		}

		const int32 RolledOfferCount = Inventory->GetOffers().Num();
		if (RolledOfferCount != AugmentOfferCount)
		{
			UE_LOG(LogBaseRoundGameMode, Error,
				TEXT("%s rolled %d/%d augment offers; the round will not open with an incomplete draft."),
				*GetNameSafe(ShooterPlayerState), RolledOfferCount, AugmentOfferCount);
			bAllRolled = false;
		}
	}

	UE_LOG(LogBaseRoundGameMode, Log, TEXT("Round %d augment draft opened. MatchSeed=%d RoundSeed=%d"), RoundNumber, MatchSeed, RoundSeed);
	return bFoundPlayer && bAllRolled;
}

bool ABaseRoundGameMode::AutoSelectAugmentOffersForPlayers()
{
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState))
	{
		return false;
	}

	bool bAllFinalized = true;
	for (APlayerState* PlayerState : RoundGameState->PlayerArray)
	{
		AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		UAugmentInventoryComponent* Inventory = IsValid(ShooterPlayerState) ? ShooterPlayerState->GetAugmentInventory() : nullptr;
		if (!IsValid(Inventory))
		{
			bAllFinalized = false;
			continue;
		}

		if (!Inventory->GetOffers().IsEmpty())
		{
			Inventory->ServerAutoSelectOffer();
		}

		if (!Inventory->GetOffers().IsEmpty())
		{
			bAllFinalized = false;
		}
	}

	return bAllFinalized;
}

bool ABaseRoundGameMode::IsAugmentRegistryReady() const
{
	if (!bEnableAugmentDraft)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	const UAugmentRegistrySubsystem* Registry = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAugmentRegistrySubsystem>()
		: nullptr;

	return IsValid(Registry) && Registry->IsReady();
}

int32 ABaseRoundGameMode::MakeRoundSeed(const int32 RoundNumber) const
{
	uint32 Seed = HashCombine(static_cast<uint32>(MatchSeed), static_cast<uint32>(FMath::Max(1, RoundNumber)));
	Seed &= 0x7fffffffu;
	return Seed == 0u ? 1 : static_cast<int32>(Seed);
}

void ABaseRoundGameMode::RegenerateMatchSeed()
{
	MatchSeed = FMath::Rand();
	if (MatchSeed == 0)
	{
		MatchSeed = 1;
	}
}

void ABaseRoundGameMode::HandleAugmentRegistryReady()
{
	bLoggedWaitingForAugmentRegistry = false;
	EvaluateRoundStart();
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
		&& PlayerStateClass->IsChildOf(AShooterPlayerState::StaticClass())
		&& SpawnClearanceRadius >= 0.0f
		&& MinimumPlayersToStart > 0
		&& InitialRoundStartDelay >= 0.0f
		&& RoundStartDelay >= 0.0f
		&& RoundEndDelay >= 0.0f
		&& RoundDuration > 0.0f
		&& (!bEnableAugmentDraft || (AugmentSelectionDuration > 0.0f && AugmentOfferCount > 0))
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
