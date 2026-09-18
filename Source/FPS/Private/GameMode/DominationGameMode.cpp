#include "GameMode/DominationGameMode.h"

#include "Augment/AugmentDraftProfile.h"
#include "Audio/DominationSystemAnnouncer.h"
#include "GameMode/Manager/DominationAnnouncerManager.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/Manager/AugmentDraftManager.h"
#include "GameMode/State/DominationGameState.h"
#include "GameMode/Manager/DominationLoadoutManager.h"
#include "GameMode/Manager/DominationMatchStartManager.h"
#include "GameMode/Manager/DominationObjectiveManager.h"
#include "GameMode/Manager/DominationRespawnManager.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameMode/Types/SurrenderVoteTypes.h"
#include "GameMode/Manager/TeamPlayerManager.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Session/SessionSubSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationGameMode, Log, All);

ADominationGameMode::ADominationGameMode()
{
	GameStateClass = ADominationGameState::StaticClass();
	PlayerControllerClass = ADominationPlayerController::StaticClass();
	PlayerStateClass = ADominationPlayerState::StaticClass();
	bDelayedStart = true;
	AnnouncerClass = ADominationSystemAnnouncer::StaticClass();
	AnnouncerManager = CreateDefaultSubobject<UDominationAnnouncerManager>(TEXT("AnnouncerManager"));

	MatchStartManager = CreateDefaultSubobject<UDominationMatchStartManager>(TEXT("MatchStartManager"));
	LoadoutManager = CreateDefaultSubobject<UDominationLoadoutManager>(TEXT("LoadoutManager"));
	ObjectiveManager = CreateDefaultSubobject<UDominationObjectiveManager>(TEXT("ObjectiveManager"));
	RespawnManager = CreateDefaultSubobject<UDominationRespawnManager>(TEXT("RespawnManager"));
	AugmentDraftManager = CreateDefaultSubobject<UAugmentDraftManager>(TEXT("AugmentDraftManager"));
}

void ADominationGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!HasValidCombatClassConfiguration())
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination match start stopped because combat class Pawn or weapon settings are incomplete."));
		return;
	}

	if (!IsValid(ObjectiveManager)
		|| !ObjectiveManager->Initialize(this, CaptureRules, PointTransitionDuration))
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination match start stopped because the five capture points are not configured correctly."));
		return;
	}

	ObjectiveManager->OnCaptureCompleted.AddUObject(
		this,
		&ThisClass::HandleCaptureCompleted);

	if (!IsValid(RespawnManager)
		|| !RespawnManager->Initialize(
			this,
			ObjectiveManager,
			RespawnDelay,
			RespawnProtectionDuration))
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination match start stopped because the spawn volumes are not configured correctly."));
		return;
	}

	if (bEnableAugmentDraft && !IsValid(AugmentDraftProfile))
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination match start stopped because no augment draft profile is configured."));
		return;
	}

	AugmentDraftManager->Initialize(this, bEnableAugmentDraft, AugmentDraftProfile);
	AugmentDraftManager->OnDependenciesReady.AddUObject(
		this,
		&ThisClass::HandleAugmentDraftDependenciesReady);
	AugmentDraftManager->OnPlayerDraftQueueDrained.AddUObject(
		this,
		&ThisClass::HandleAugmentDraftQueueDrained);
	if (!AugmentDraftManager->HasRequiredDependencies())
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination match start stopped because the augment draft subsystem is missing."));
		return;
	}

	// 음성 설정 실패는 매치를 막지 않는다. 매치 시작 이벤트보다 먼저 구독한다.
	if (!IsValid(AnnouncerManager) || !AnnouncerManager->Initialize(this, AnnouncerClass))
	{
		UE_LOG(LogDominationGameMode, Warning, TEXT("아나운서 초기화 실패. 음성 안내 없이 도미네이션을 진행합니다."));
	}

	if (!IsValid(MatchStartManager)
		|| !MatchStartManager->Initialize(
			this,
			MinimumPlayersToStart,
			MinimumPlayersPerTeam,
			ClassSelectionDuration))
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination match start stopped because the match-start manager could not be initialized."));
		return;
	}

	MatchStartManager->OnInitialPlayersSpawned.AddUObject(
		this,
		&ThisClass::HandleInitialPlayersSpawned);

	// 로비 -> 게임 이동 중에는 방을 닫아두고, 실제 Domination 월드 준비가 끝난 뒤
	// 세션 검색/참가를 다시 열어 JIP가 게임플레이 수명주기와 같은 시점부터 허용되게 한다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USessionSubSystem* SessionSubsystem =
			GameInstance->GetSubsystem<USessionSubSystem>())
		{
			SessionSubsystem->SetRoomJoinable(true);
		}
	}
}

void ADominationGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(MatchEndTimerHandle);

	// 다른 매니저가 상태를 초기화하기 전에 구독과 재생을 정리한다.
	if (IsValid(AnnouncerManager))
	{
		AnnouncerManager->Shutdown();
	}

	AugmentDraftManager->OnDependenciesReady.RemoveAll(this);
	AugmentDraftManager->OnPlayerDraftQueueDrained.RemoveAll(this);
	AugmentDraftManager->Shutdown();
	PendingInitialAugmentPlayers.Reset();

	if (IsValid(ObjectiveManager))
	{
		ObjectiveManager->OnCaptureCompleted.RemoveAll(this);
	}

	if (IsValid(MatchStartManager))
	{
		MatchStartManager->OnInitialPlayersSpawned.RemoveAll(this);
		MatchStartManager->Shutdown();
	}

	if (IsValid(RespawnManager))
	{
		RespawnManager->Shutdown();
	}

	if (IsValid(ObjectiveManager))
	{
		ObjectiveManager->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}

ASystemAnnouncer* ADominationGameMode::GetSystemAnnouncer() const
{
	return IsValid(AnnouncerManager) ? AnnouncerManager->GetSystemAnnouncer() : nullptr;
}

void ADominationGameMode::Logout(AController* Exiting)
{
	AShooterPlayerState* ExitingPlayerState = IsValid(Exiting)
		? Exiting->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	if (IsValid(ExitingPlayerState)
		&& PendingInitialAugmentPlayers.Remove(ExitingPlayerState) > 0)
	{
		TryStartInitialObjectiveTransition();
	}

	Super::Logout(Exiting);
}

void ADominationGameMode::NotifyPlayerEliminatedBy(
	AController* EliminatedController,
	AShooterPlayerState*)
{
	const ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	if (!HasAuthority()
		|| !IsValid(EliminatedController)
		|| !IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::InProgress
		|| !IsValid(RespawnManager))
	{
		return;
	}

	if (AShooterPlayerState* PlayerState =
		EliminatedController->GetPlayerState<AShooterPlayerState>())
	{
		PlayerState->SetParticipationState(
			EPlayerParticipationState::WaitingForNextSpawn);
		PlayerState->SetAlive(false);
	}

	MovePlayerToSpectator(Cast<APlayerController>(EliminatedController));
	RespawnManager->ScheduleRespawn(EliminatedController);
}

void ADominationGameMode::NotifyPlayerReadyForGameFlow(APlayerController* PlayerController)
{
	ADominationPlayerController* DominationPlayerController =
		Cast<ADominationPlayerController>(PlayerController);

	if (!IsValid(DominationPlayerController))
	{
		return;
	}

	Super::NotifyPlayerReadyForGameFlow(PlayerController);

	ADominationPlayerState* PlayerState =
		DominationPlayerController->GetPlayerState<ADominationPlayerState>();
	if (!IsValid(PlayerState))
	{
		return;
	}

	if (PlayerState->GetParticipationState()
		== EPlayerParticipationState::WaitingForNextSpawn)
	{
		DominationPlayerController->ClientHideLoadingScreen();
		return;
	}

	if (!AreAugmentDraftDependenciesReady())
	{
		return;
	}

	if (IsValid(MatchStartManager))
	{
		MatchStartManager->NotifyPlayerLoaded();
	}
}

bool ADominationGameMode::IsSurrenderVoteAllowedNow() const
{
	const ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	return Super::IsSurrenderVoteAllowedNow()
		&& IsValid(DominationGameState)
		&& DominationGameState->GetDominationPhase() == EDominationMatchPhase::InProgress;
}

void ADominationGameMode::HandleSurrenderApproved(const EGameTeam SurrenderingTeam)
{
	const EGameTeam WinningTeam = SurrenderVoteRules::GetOpposingTeam(SurrenderingTeam);
	if (WinningTeam != EGameTeam::None)
	{
		FinishDominationMatch(WinningTeam);
	}
}

void ADominationGameMode::RequestCombatClass(
	ADominationPlayerController* PlayerController,
	const EDominationCombatClass RequestedClass)
{
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	if (!HasAuthority() || !IsValid(PlayerState))
	{
		return;
	}

	if (PlayerState->GetParticipationState()
		== EPlayerParticipationState::WaitingForMatchStart)
	{
		if (IsValid(MatchStartManager))
		{
			MatchStartManager->RequestCombatClass(PlayerController, RequestedClass);
		}
		return;
	}

	TryJoinPlayerInProgress(PlayerController, RequestedClass);
}

void ADominationGameMode::RequestPreviewCombatClass(
	ADominationPlayerController* PlayerController,
	const EDominationCombatClass RequestedClass)
{
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	// WaitingForMatchStart가 아니면 아무것도 하지 않는다
	if (!HasAuthority()
		|| !IsValid(PlayerState)
		|| PlayerState->GetParticipationState()
			!= EPlayerParticipationState::WaitingForMatchStart)
	{
		return;
	}

	if (IsValid(MatchStartManager))
	{
		MatchStartManager->RequestPreviewCombatClass(PlayerController, RequestedClass);
	}
}

UDominationLoadoutManager* ADominationGameMode::GetLoadoutManager() const
{
	return LoadoutManager.Get();
}

void ADominationGameMode::GetInitialParticipantPlayerStates(
	TArray<ADominationPlayerState*>& OutPlayerStates) const
{
	OutPlayerStates.Reset();
	if (IsValid(MatchStartManager))
	{
		MatchStartManager->GetInitialParticipantPlayerStates(OutPlayerStates);
	}
}

bool ADominationGameMode::RestartPlayerAtObjective(
	APlayerController* PlayerController,
	const EDominationPointId ObjectivePointId)
{
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	FTransform SpawnTransform;
	if (!IsValid(PlayerState)
		|| !IsValid(RespawnManager)
		|| !RespawnManager->TryGetSpawnTransform(
			PlayerState->GetTeam(),
			ObjectivePointId,
			SpawnTransform))
	{
		return false;
	}

	if (IsValid(PlayerController->GetPawn()))
	{
		if (UTeamPlayerManager* PlayerManager = GetTeamPlayerManager())
		{
			PlayerManager->RemovePlayerPawn(PlayerController);
		}
	}

	RestartPlayerAtTransform(PlayerController, SpawnTransform);
	APawn* SpawnedPawn = PlayerController->GetPawn();
	return IsValid(SpawnedPawn) && SpawnedPawn->GetController() == PlayerController;
}

void ADominationGameMode::RespawnPlayer(AController* Controller)
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	const ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	if (!IsValid(PlayerController)
		|| !IsValid(PlayerState)
		|| !IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::InProgress
		|| PlayerState->GetParticipationState()
			!= EPlayerParticipationState::WaitingForNextSpawn)
	{
		return;
	}

	const EDominationPointId SpawnObjectivePointId = DominationGameState->HasActivePoint()
		? DominationGameState->GetActivePointId()
		: EDominationPointId::C;

	if (!RestartPlayerAtObjective(
		PlayerController,
		SpawnObjectivePointId))
	{
		UE_LOG(
			LogDominationGameMode,
			Warning,
			TEXT("Domination respawn candidate is temporarily unavailable for team %d at active point %d. Retrying."),
			static_cast<uint8>(PlayerState->GetTeam()),
			static_cast<uint8>(SpawnObjectivePointId));
		if (IsValid(RespawnManager))
		{
			RespawnManager->ScheduleRespawnRetry(PlayerController);
		}
		return;
	}

	APawn* SpawnedPawn = PlayerController->GetPawn();
	PlayerState->ServerRespawnReset();
	if (!IsValid(LoadoutManager)
		|| !LoadoutManager->ApplyCombatClassLoadout(PlayerState, SpawnedPawn))
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Failed to restore the Domination class loadout for %s."),
			*GetNameSafe(PlayerState));
		MovePlayerToSpectator(PlayerController);
		PlayerState->SetAlive(false);
		return;
	}

	PlayerController->ChangeState(NAME_Playing);
	PlayerController->ClientGotoState(NAME_Playing);
	if (AShooterPlayerController* ShooterPlayerController = Cast<AShooterPlayerController>(PlayerController))
	{
		ShooterPlayerController->ClientExitSpectatorMode();
	}
	PlayerState->SetParticipationState(EPlayerParticipationState::Active);
	RespawnManager->BeginRespawnProtection(PlayerState);

	ServeNextAugmentReward(PlayerState);
}

bool ADominationGameMode::TryJoinPlayerInProgress(
	ADominationPlayerController* PlayerController,
	const EDominationCombatClass RequestedClass)
{
	ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	if (!IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::InProgress
		|| !IsValid(PlayerState)
		|| !PlayerState->IsLoadedIntoMatch()
		|| PlayerState->GetParticipationState()
			!= EPlayerParticipationState::WaitingForNextSpawn
		|| !IsValid(LoadoutManager)
		|| !IsValid(RespawnManager)
		|| !PlayerState->TryConfirmCombatClass(RequestedClass))
	{
		return false;
	}

	if (!LoadoutManager->InitializeCombatClassInventory(PlayerState))
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Failed to initialize the Domination class inventory for join-in-progress player %s."),
			*GetNameSafe(PlayerState));
		return false;
	}

	RespawnManager->ScheduleRespawn(PlayerController);
	UE_LOG(LogDominationGameMode, Log,
		TEXT("Join-in-progress player %s selected class %d and is waiting to spawn."),
		*GetNameSafe(PlayerState),
		static_cast<uint8>(RequestedClass));
	return true;
}

bool ADominationGameMode::HasValidCombatClassConfiguration() const
{
	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();

	return IsValid(ClassSettings)
		&& ClassSettings->HasValidDefinitions()
		&& IsValid(LoadoutManager)
		&& LoadoutManager->HasRequiredWeaponRegistryEntries();
}

bool ADominationGameMode::AreAugmentDraftDependenciesReady() const
{
	return !bEnableAugmentDraft
		|| AugmentDraftManager->AreDependenciesReady();
}

void ADominationGameMode::HandleAugmentDraftDependenciesReady()
{
	if (HasAuthority() && IsValid(MatchStartManager))
	{
		MatchStartManager->NotifyPlayerLoaded();
	}
}

void ADominationGameMode::GrantAugmentRewardsForRound(
	const int32 RewardRoundNumber)
{
	if (!bEnableAugmentDraft)
	{
		return;
	}

	switch (RewardRoundNumber)
	{
	case 1:
		AugmentDraftManager->GrantToAll(
			EAugmentRewardPool::Active,
			RewardRoundNumber,
			FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.Ability.Slot1"))));
		break;
	case 2:
		AugmentDraftManager->GrantToAll(
			EAugmentRewardPool::Active,
			RewardRoundNumber,
			FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.Ability.Slot2"))));
		break;
	case 3:
		AugmentDraftManager->GrantToAll(
			EAugmentRewardPool::Active,
			RewardRoundNumber,
			FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.Ability.Slot3"))));
		break;
	case 4:
	case 5:
		break;
	default:
		return;
	}

	AugmentDraftManager->GrantToAll(
		EAugmentRewardPool::Passive,
		RewardRoundNumber);
}

void ADominationGameMode::ServeNextAugmentReward(
	AShooterPlayerState* PlayerState)
{
	if (bEnableAugmentDraft)
	{
		AugmentDraftManager->ServeNextReward(PlayerState);
	}
}

void ADominationGameMode::HandleInitialPlayersSpawned()
{
	bInitialObjectiveTransitionStarted = false;
	PendingInitialAugmentPlayers.Reset();

	GrantAugmentRewardsForRound(1);

	TArray<ADominationPlayerState*> InitialPlayerStates;
	GetInitialParticipantPlayerStates(InitialPlayerStates);

	if (!bEnableAugmentDraft)
	{
		TryStartInitialObjectiveTransition();
		return;
	}

	// 먼저 전체 참가자를 등록한 뒤 후보를 제시한다.
	// ServeNextReward가 동기적으로 대기열 소진을 알리더라도
	// 일부 플레이어만 등록된 상태에서 중앙 거점 타이머가 시작되지 않게 한다.
	for (ADominationPlayerState* PlayerState : InitialPlayerStates)
	{
		if (IsValid(PlayerState))
		{
			PendingInitialAugmentPlayers.Add(PlayerState);
		}
	}

	for (ADominationPlayerState* PlayerState : InitialPlayerStates)
	{
		ServeNextAugmentReward(PlayerState);
	}

	TryStartInitialObjectiveTransition();
}

void ADominationGameMode::HandleAugmentDraftQueueDrained(
	AShooterPlayerState* PlayerState)
{
	if (!HasAuthority() || !IsValid(PlayerState))
	{
		return;
	}

	if (PendingInitialAugmentPlayers.Remove(PlayerState) == 0)
	{
		return;
	}

	UE_LOG(LogDominationGameMode, Log,
		TEXT("Initial augment draft completed for %s. Waiting for %d player(s)."),
		*GetNameSafe(PlayerState),
		PendingInitialAugmentPlayers.Num());

	TryStartInitialObjectiveTransition();
}

void ADominationGameMode::TryStartInitialObjectiveTransition()
{
	ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	if (!HasAuthority()
		|| bInitialObjectiveTransitionStarted
		|| !PendingInitialAugmentPlayers.IsEmpty()
		|| !IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::InProgress)
	{
		return;
	}

	if (!IsValid(ObjectiveManager) || !ObjectiveManager->ActivateInitialPoint())
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination failed to begin the initial center-point transition."));
		return;
	}

	bInitialObjectiveTransitionStarted = true;
	UE_LOG(LogDominationGameMode, Log,
		TEXT("All initial augment drafts are complete. Starting the center-point transition."));
	OnDominationMatchStarted.Broadcast();
}

void ADominationGameMode::HandleCaptureCompleted(
	const EDominationPointId CapturedPointId,
	const EGameTeam CapturingTeam)
{
	ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	if (!HasAuthority()
		|| !IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::InProgress)
	{
		return;
	}

	DominationGameState->AddTeamScore(CapturingTeam);
	const int32 TeamScore = DominationGameState->GetTeamScore(CapturingTeam);
	const bool bMatchWon = TeamScore >= DominationGameState->GetRoundsToWin();
	if (!bMatchWon)
	{
		GrantAugmentRewardsForRound(
			DominationGameState->GetCompletedRoundCount() + 1);
	}

	// 점수/매치 승패를 먼저 확정한 뒤 ObjectiveManager에 다음 전선 진행을 명시적으로 지시한다.
	// 매치 승리 점령도 GameState에 기록해 최종 점령 피드백이 클라이언트에 남도록 한다.
	if (bMatchWon)
	{
		DominationGameState->RecordObjectiveCapture(
			CapturedPointId,
			CapturingTeam);
	}
	else if (IsValid(ObjectiveManager))
	{
		ObjectiveManager->AdvanceAfterCapture(
			CapturedPointId,
			CapturingTeam);
	}

	UE_LOG(
		LogDominationGameMode,
		Log,
		TEXT("Team %d won round %d at point %d. Score: TeamA %d - TeamB %d."),
		static_cast<uint8>(CapturingTeam),
		DominationGameState->GetCompletedRoundCount(),
		static_cast<uint8>(CapturedPointId),
		DominationGameState->GetTeamAScore(),
		DominationGameState->GetTeamBScore());

	OnRoundCompleted.Broadcast(
		CapturedPointId,
		CapturingTeam,
		TeamScore,
		bMatchWon);

	if (bMatchWon)
	{
		FinishDominationMatch(CapturingTeam);
	}
}

void ADominationGameMode::FinishDominationMatch(const EGameTeam WinningTeam)
{
	ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	if (!HasAuthority()
		|| !IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::InProgress)
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USessionSubSystem* SessionSubsystem =
			GameInstance->GetSubsystem<USessionSubSystem>())
		{
			SessionSubsystem->SetRoomJoinable(false);
		}
	}

	DominationGameState->SetMatchWinner(WinningTeam);
	DominationGameState->SetDominationPhase(
		EDominationMatchPhase::MatchEnded,
		MatchEndDelay);

	if (IsValid(ObjectiveManager))
	{
		ObjectiveManager->StopObjectiveFlow();
	}
	if (IsValid(RespawnManager))
	{
		RespawnManager->CancelPendingRespawns();
		RespawnManager->ClearAllRespawnProtections();
	}
	AugmentDraftManager->CancelDraft();

	EndMatch();
	UE_LOG(
		LogDominationGameMode,
		Log,
		TEXT("Domination match ended. Winner: Team %d, final score: TeamA %d - TeamB %d."),
		static_cast<uint8>(WinningTeam),
		DominationGameState->GetTeamAScore(),
		DominationGameState->GetTeamBScore());

	if (MatchEndDelay <= 0.0f)
	{
		ReturnToLobbyAfterMatch();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			MatchEndTimerHandle,
			this,
			&ThisClass::ReturnToLobbyAfterMatch,
			MatchEndDelay,
			false);
	}
}

void ADominationGameMode::ReturnToLobbyAfterMatch()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USessionSubSystem* SessionSubsystem =
			GameInstance->GetSubsystem<USessionSubSystem>())
		{
			if (SessionSubsystem->ReturnRoomToLobby())
			{
				return;
			}
		}
	}

	ReturnToMainMenuHost();
}

void ADominationGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// BaseTeamGameMode의 Super 경로는 즉시 Pawn을 생성하므로 팀 배정만 수행한다.
	if (!InitializeTeamPlayer(NewPlayer))
	{
		return;
	}

	const EPlayerParticipationState NewPlayerParticipationState = ResolveParticipationStateForNewPlayer(NewPlayer);
	if (AShooterPlayerState* PlayerState = NewPlayer->GetPlayerState<AShooterPlayerState>())
	{
		PlayerState->SetParticipationState(NewPlayerParticipationState);
		AugmentDraftManager->ResetPlayerForNewMatch(PlayerState);
	}

	if (NewPlayerParticipationState == EPlayerParticipationState::WaitingForNextSpawn)
	{
		MovePlayerToSpectator(NewPlayer);
		if (AShooterPlayerState* PlayerState =
			NewPlayer->GetPlayerState<AShooterPlayerState>())
		{
			// SpectatorPawn이 첫 Avatar가 되면 초기 vitals가 생존 상태를 만들 수 있으므로
			// 실제 전투 Pawn이 생기기 전까지 참가 상태와 생존 상태를 일치시킨다.
			PlayerState->SetAlive(false);
		}
	}
}

EPlayerParticipationState ADominationGameMode::ResolveParticipationStateForNewPlayer(APlayerController*) const
{
	const ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	const EDominationMatchPhase Phase = IsValid(DominationGameState)
		? DominationGameState->GetDominationPhase()
		: EDominationMatchPhase::WaitingForPlayers;

	if (Phase == EDominationMatchPhase::WaitingForPlayers)
	{
		return EPlayerParticipationState::WaitingForMatchStart;
	}

	return EPlayerParticipationState::WaitingForNextSpawn;
}

UClass* ADominationGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const ADominationPlayerState* DominationPlayerState = IsValid(InController)
		? InController->GetPlayerState<ADominationPlayerState>()
		: nullptr;

	if (!IsValid(DominationPlayerState) || !DominationPlayerState->HasSelectedCombatClass())
	{
		return nullptr;
	}

	const EDominationCombatClass CombatClass = DominationPlayerState->GetCombatClass();
	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	UClass* PawnClass = IsValid(ClassSettings)
		? ClassSettings->ResolvePawnClass(CombatClass)
		: nullptr;
	if (!IsValid(PawnClass))
	{
		UE_LOG(
			LogDominationGameMode,
			Error,
			TEXT("No Pawn class is configured for Domination combat class %d."),
			static_cast<uint8>(CombatClass));
		return nullptr;
	}

	return PawnClass;
}
