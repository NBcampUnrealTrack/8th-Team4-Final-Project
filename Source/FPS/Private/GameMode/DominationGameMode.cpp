#include "GameMode/DominationGameMode.h"

#include "Actor/DominationCapturePoint.h"
#include "Augment/AugmentDraftProfile.h"
#include "Augment/AugmentInventoryComponent.h"
#include "Audio/DominationSystemAnnouncer.h"
#include "Character/ShooterCharacter.h"
#include "GameMode/Manager/DominationAnnouncerManager.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/Manager/AugmentDraftManager.h"
#include "GameMode/State/DominationGameState.h"
#include "GameMode/Manager/DominationLoadoutManager.h"
#include "GameMode/Manager/DominationMatchStartManager.h"
#include "GameMode/Manager/DominationObjectiveManager.h"
#include "GameMode/Manager/DominationRespawnManager.h"
#include "GameMode/Manager/DominationSandstormManager.h"
#include "GameMode/Settings/DominationSandstormSettings.h"
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
	SandstormManager = CreateDefaultSubobject<UDominationSandstormManager>(TEXT("SandstormManager"));
	AugmentDraftManager = CreateDefaultSubobject<UAugmentDraftManager>(TEXT("AugmentDraftManager"));
}

void ADominationGameMode::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogDominationGameMode, Log, TEXT("[DominationTravel] Stage=DestinationBeginPlay World=%s"),
		*GetNameSafe(GetWorld()));
	LoadoutManager->OnCombatAssetsReady.AddUObject(this, &ThisClass::HandleCombatAssetsReady);
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

	if (!SandstormManager->Initialize(this, SandstormSettings))
	{
		UE_LOG(LogDominationGameMode, Warning,
			TEXT("Assign a Sandstorm Settings asset in the Domination GameMode to enable storms."));
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
	GetWorldTimerManager().ClearAllTimersForObject(this);
	GetWorldTimerManager().ClearTimer(MatchEndTimerHandle);
	SandstormManager->Shutdown();

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
	PendingCombatAssetRespawns.Reset();
	LoadoutManager->Shutdown();

	Super::EndPlay(EndPlayReason);
}

ASystemAnnouncer* ADominationGameMode::GetSystemAnnouncer() const
{
	return IsValid(AnnouncerManager) ? AnnouncerManager->GetSystemAnnouncer() : nullptr;
}

void ADominationGameMode::Logout(AController* Exiting)
{
	PendingCombatAssetRespawns.Remove(Cast<APlayerController>(Exiting));
	MatchStartManager->NotifyPlayerLeaving(Cast<APlayerController>(Exiting));
	AShooterPlayerState* ExitingPlayerState = IsValid(Exiting)
		? Exiting->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	if (const ADominationPlayerState* DominationPlayerState = Cast<ADominationPlayerState>(ExitingPlayerState))
	{
		CaptureReconnectSnapshot(DominationPlayerState);
	}
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

	const ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	if (PlayerState->GetParticipationState() == EPlayerParticipationState::WaitingForNextSpawn
		&& IsValid(DominationGameState)
		&& DominationGameState->GetDominationPhase() != EDominationMatchPhase::WaitingForPlayers)
	{
		if (PlayerState->HasSelectedCombatClass())
		{
			TryJoinPlayerInProgress(DominationPlayerController, PlayerState->GetCombatClass());
		}
		DominationPlayerController->ClientFinishLoadingScreen();
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

void ADominationGameMode::NotifyClientInitialCombatAssetsReady(
	ADominationPlayerController* PlayerController, const FGuid Generation, const bool bSucceeded)
{
	if (HasAuthority() && IsValid(MatchStartManager))
	{
		MatchStartManager->NotifyClientInitialCombatAssetsReady(PlayerController, Generation, bSucceeded);
	}
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
	const bool bSpawned = IsValid(SpawnedPawn) && SpawnedPawn->GetController() == PlayerController;
	UE_LOG(LogDominationGameMode, Log,
		TEXT("[DominationTravel] Stage=PlayerSpawn Player=%s Pawn=%s Team=%d Objective=%d Success=%d"),
		*GetNameSafe(PlayerController), *GetNameSafe(SpawnedPawn),
		static_cast<uint8>(PlayerState->GetTeam()), static_cast<uint8>(ObjectivePointId), bSpawned);
	return bSpawned;
}

// 거점 기믹(중계 비콘)의 팀별 부활 대기 감소를 RespawnManager로 전달
void ADominationGameMode::SetTeamRespawnDelayReduction(
	const EGameTeam Team,
	const float Seconds)
{
	if (IsValid(RespawnManager))
	{
		RespawnManager->SetTeamRespawnDelayReduction(Team, Seconds);
	}
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

	const EDominationCombatAssetStatus AssetStatus =
		LoadoutManager->EnsureCombatAssets(PlayerState->GetCombatClass());
	if (AssetStatus == EDominationCombatAssetStatus::Loading)
	{
		PendingCombatAssetRespawns.Add(PlayerController);
		return;
	}
	if (AssetStatus == EDominationCombatAssetStatus::Failed)
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination respawn stopped: selected combat assets failed to load for %s."),
			*GetNameSafe(PlayerState));
		return;
	}

	if (!DominationGameState->HasActivePoint())
	{
		UE_LOG(
			LogDominationGameMode,
			Verbose,
			TEXT("Domination respawn is waiting for the next active objective."));
		if (IsValid(RespawnManager))
		{
			RespawnManager->ScheduleRespawnRetry(PlayerController);
		}
		return;
	}

	const EDominationPointId SpawnObjectivePointId = DominationGameState->GetActivePointId();

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

	if (!PrepareCombatPawn(PlayerController))
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Failed to restore the Domination class loadout for %s. Respawn will retry."),
			*GetNameSafe(PlayerState));
		MovePlayerToSpectator(PlayerController);
		PlayerState->SetAlive(false);
		if (IsValid(RespawnManager))
		{
			RespawnManager->ScheduleRespawnRetry(PlayerController);
		}
		return;
	}

	ActivateCombatPlayer(PlayerController);
	RespawnManager->BeginRespawnProtection(PlayerState);

	ServeNextAugmentReward(PlayerState);
}

void ADominationGameMode::HandleCombatAssetsReady(
	const EDominationCombatClass CombatClass, const bool bSucceeded)
{
	TArray<TWeakObjectPtr<APlayerController>> PlayersToResume;
	for (auto Iterator = PendingCombatAssetRespawns.CreateIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		const ADominationPlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ADominationPlayerState>() : nullptr;
		if (!IsValid(PlayerState))
		{
			Iterator.RemoveCurrent();
		}
		else if (PlayerState->GetCombatClass() == CombatClass)
		{
			PlayersToResume.Add(PlayerController);
			Iterator.RemoveCurrent();
		}
	}
	for (const TWeakObjectPtr<APlayerController>& PlayerController : PlayersToResume)
	{
		if (bSucceeded)
		{
			RespawnPlayer(PlayerController.Get());
		}
		else
		{
			UE_LOG(LogDominationGameMode, Error,
				TEXT("Domination respawn stopped: selected combat assets failed to load for %s."),
				*GetNameSafe(PlayerController.Get()));
		}
	}
}

bool ADominationGameMode::PrepareCombatPawn(APlayerController* PlayerController)
{
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>() : nullptr;
	APawn* Pawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
	if (!IsValid(PlayerState) || !IsValid(Pawn) || Pawn->GetController() != PlayerController
		|| LoadoutManager->EnsureCombatAssets(PlayerState->GetCombatClass())
			!= EDominationCombatAssetStatus::Ready)
	{
		return false;
	}

	PlayerState->ServerRespawnReset();
	return PlayerState->IsAlive()
		&& LoadoutManager->EnsureCombatClassInventory(PlayerState)
		&& LoadoutManager->ApplyCombatClassLoadout(PlayerState, Pawn);
}

void ADominationGameMode::ActivateCombatPlayer(APlayerController* PlayerController)
{
	ADominationPlayerState* PlayerState = PlayerController->GetPlayerState<ADominationPlayerState>();
	PlayerController->ChangeState(NAME_Playing);
	PlayerController->ClientGotoState(NAME_Playing);
	if (AShooterPlayerController* ShooterPlayerController = Cast<AShooterPlayerController>(PlayerController))
	{
		ShooterPlayerController->ClientExitSpectatorMode();
	}
	PlayerState->SetParticipationState(EPlayerParticipationState::Active);
	UE_LOG(LogDominationGameMode, Log, TEXT("[DominationTravel] Stage=CombatActivated Player=%s Pawn=%s"),
		*GetNameSafe(PlayerController), *GetNameSafe(PlayerController->GetPawn()));
}

void ADominationGameMode::RetryDominationMatchStart()
{
	if (HasAuthority())
	{
		MatchStartManager->RetryMatchStart();
	}
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
		|| (PlayerState->HasSelectedCombatClass()
			? PlayerState->GetCombatClass() != RequestedClass
			: !PlayerState->TryConfirmCombatClass(RequestedClass)))
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

	if (bEnableAugmentDraft && IsValid(AugmentDraftManager))
	{
		PendingLateJoinAugmentPlayers.Add(PlayerState);
		AugmentDraftManager->EnqueueMissingRewards(PlayerState);
		if (AugmentDraftManager->ServeNextReward(PlayerState))
		{
			UE_LOG(LogDominationGameMode, Log,
				TEXT("Late join player %s is selecting missed augments before spawn."),
				*GetNameSafe(PlayerState));
			return true;
		}
		// 보상이 없으면 즉시 QueueDrained 콜백이 왔을 수 있으므로, 아래 예약도 안전하다.
		PendingLateJoinAugmentPlayers.Remove(PlayerState);
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

	if (PendingLateJoinAugmentPlayers.Remove(PlayerState) > 0)
	{
		if (ADominationPlayerController* Controller = Cast<ADominationPlayerController>(PlayerState->GetOwner()))
		{
			RespawnManager->ScheduleRespawn(Controller);
			UE_LOG(LogDominationGameMode, Log,
				TEXT("Late join augment catch-up completed for %s. Spawn scheduled."),
				*GetNameSafe(PlayerState));
		}
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
	SandstormManager->StartScheduling();
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

	SandstormManager->StopStormFlow();

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

AActor* ADominationGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// 입장 시 컨트롤러 위치만 정한다. 전투 Pawn은 RespawnManager의 팀별 볼륨에서 생성한다.
	// 이 조회는 BeginPlay 이전에도 호출되므로 아직 초기화되지 않은 매니저에 의존하지 않는다.
	for (TActorIterator<ADominationCapturePoint> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ADominationCapturePoint* CapturePoint = *Iterator;
		if (IsValid(CapturePoint) && CapturePoint->GetPointId() == DominationRules::InitialPointId)
		{
			UE_LOG(LogDominationGameMode, Log,
				TEXT("[DominationTravel] Stage=ControllerArrival Controller=%s Anchor=%s"),
				*GetNameSafe(Player), *GetNameSafe(CapturePoint));
			return CapturePoint;
		}
	}

	UE_LOG(LogDominationGameMode, Error,
		TEXT("Domination controller arrival failed: initial capture point %d is not loaded in %s. "
			"Keep the capture points and spawn volumes always loaded in the map."),
		static_cast<uint8>(DominationRules::InitialPointId), *GetNameSafe(GetWorld()));
	return nullptr;
}

void ADominationGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// BaseTeamGameMode의 Super 경로는 즉시 Pawn을 생성하므로 팀 배정만 수행한다.
	ADominationPlayerState* DominationPlayerState = IsValid(NewPlayer)
		? NewPlayer->GetPlayerState<ADominationPlayerState>() : nullptr;
	const bool bIsReconnect = RestoreReconnectSnapshot(DominationPlayerState);
	if (!bIsReconnect && !InitializeTeamPlayer(NewPlayer))
	{
		return;
	}
	if (bIsReconnect)
	{
		// 복원한 팀은 유지하되, TeamPlayerManager의 로스터에는 새 Controller를 다시 등록한다.
		InitializeTeamPlayer(NewPlayer);
	}

	const EPlayerParticipationState NewPlayerParticipationState = ResolveParticipationStateForNewPlayer(NewPlayer);
	if (AShooterPlayerState* PlayerState = NewPlayer->GetPlayerState<AShooterPlayerState>())
	{
		PlayerState->SetParticipationState(NewPlayerParticipationState);
		if (!bIsReconnect)
		{
			AugmentDraftManager->ResetPlayerForNewMatch(PlayerState);
		}
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

FString ADominationGameMode::GetReconnectKey(const AShooterPlayerState* PlayerState) const
{
	if (!IsValid(PlayerState))
	{
		return FString();
	}

	const TSharedPtr<const FUniqueNetId> NetId = PlayerState->GetUniqueId().GetUniqueNetId();
	return NetId.IsValid() ? NetId->ToString() : FString();
}

void ADominationGameMode::PurgeExpiredReconnectSnapshots()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	for (auto It = ReconnectSnapshots.CreateIterator(); It; ++It)
	{
		if (It.Value().ExpireServerTime <= Now)
		{
			It.RemoveCurrent();
		}
	}
}

void ADominationGameMode::CaptureReconnectSnapshot(const ADominationPlayerState* PlayerState)
{
	const ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	const FString Key = GetReconnectKey(PlayerState);
	if (Key.IsEmpty() || !IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::InProgress)
	{
		return;
	}

	UAugmentInventoryComponent* Inventory = PlayerState->GetAugmentInventory();
	if (!IsValid(Inventory))
	{
		return;
	}

	PurgeExpiredReconnectSnapshots();
	FDominationReconnectSnapshot& Snapshot = ReconnectSnapshots.FindOrAdd(Key);
	Snapshot.Team = PlayerState->GetTeam();
	Snapshot.CombatClass = PlayerState->GetCombatClass();
	Snapshot.ExpireServerTime = GetWorld()->GetTimeSeconds() + ReconnectGracePeriod;
	Inventory->CaptureReconnectSnapshot(Snapshot.AugmentInventory);
}

bool ADominationGameMode::RestoreReconnectSnapshot(ADominationPlayerState* PlayerState)
{
	PurgeExpiredReconnectSnapshots();
	const FString Key = GetReconnectKey(PlayerState);
	FDominationReconnectSnapshot* Snapshot = Key.IsEmpty() ? nullptr : ReconnectSnapshots.Find(Key);
	if (!IsValid(PlayerState) || !Snapshot)
	{
		return false;
	}

	PlayerState->SetTeam(Snapshot->Team);
	PlayerState->RestoreCombatClassForReconnect(Snapshot->CombatClass);
	FText Failure;
	if (!PlayerState->GetAugmentInventory()->RestoreReconnectSnapshot(Snapshot->AugmentInventory, Failure))
	{
		UE_LOG(LogDominationGameMode, Warning, TEXT("Reconnect augment restore failed for %s: %s"),
			*GetNameSafe(PlayerState), *Failure.ToString());
		return false;
	}
	ReconnectSnapshots.Remove(Key);
	return true;
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
	const FDominationCombatClassDefinition* Definition = IsValid(ClassSettings)
		? ClassSettings->FindDefinition(CombatClass) : nullptr;
	UClass* PawnClass = Definition ? Definition->PawnClass.Get() : nullptr;
	if (!IsValid(PawnClass))
	{
		UE_LOG(
			LogDominationGameMode,
			Error,
			TEXT("Domination combat class %d was requested before its Pawn asset became ready."),
			static_cast<uint8>(CombatClass));
		return nullptr;
	}

	return PawnClass;
}

void ADominationGameMode::ForceStorm()
{
#if !UE_BUILD_SHIPPING
	if (HasAuthority() && bInitialObjectiveTransitionStarted)
	{
		SandstormManager->ForceStorm();
	}
#endif
}
