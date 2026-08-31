#include "GameMode/DominationGameMode.h"

#include "Augment/AugmentDraftProfile.h"
#include "Character/ShooterCharacter.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/AugmentDraftManager.h"
#include "GameMode/DominationGameState.h"
#include "GameMode/DominationLoadoutManager.h"
#include "GameMode/DominationMatchStartManager.h"
#include "GameMode/DominationObjectiveManager.h"
#include "GameMode/DominationRespawnManager.h"
#include "GameMode/SurrenderVoteTypes.h"
#include "GameMode/TeamPlayerManager.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"
#include "Session/SessionSubSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationGameMode, Log, All);

ADominationGameMode::ADominationGameMode()
{
	GameStateClass = ADominationGameState::StaticClass();
	PlayerControllerClass = ADominationPlayerController::StaticClass();
	PlayerStateClass = ADominationPlayerState::StaticClass();
	bDelayedStart = true;

	MatchStartManager = CreateDefaultSubobject<UDominationMatchStartManager>(TEXT("MatchStartManager"));
	LoadoutManager = CreateDefaultSubobject<UDominationLoadoutManager>(TEXT("LoadoutManager"));
	ObjectiveManager = CreateDefaultSubobject<UDominationObjectiveManager>(TEXT("ObjectiveManager"));
	RespawnManager = CreateDefaultSubobject<UDominationRespawnManager>(TEXT("RespawnManager"));
	AugmentDraftManager = CreateDefaultSubobject<UAugmentDraftManager>(TEXT("AugmentDraftManager"));

	CombatClassPawnClasses.Add(EDominationCombatClass::Trooper, nullptr);
	CombatClassPawnClasses.Add(EDominationCombatClass::Ranger, nullptr);
	CombatClassPawnClasses.Add(EDominationCombatClass::Vanguard, nullptr);
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
	if (!AugmentDraftManager->HasRequiredDependencies())
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination match start stopped because the augment draft subsystem is missing."));
		return;
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
}

void ADominationGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(MatchEndTimerHandle);

	AugmentDraftManager->OnDependenciesReady.RemoveAll(this);
	AugmentDraftManager->Shutdown();

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

	RespawnManager->ScheduleRespawn(EliminatedController);
}

void ADominationGameMode::NotifyPlayerReadyForGameFlow(APlayerController* PlayerController)
{
	ADominationPlayerController* DominationPlayerController =
		Cast<ADominationPlayerController>(PlayerController);

	if (!IsValid(DominationPlayerController) || !IsValid(MatchStartManager))
	{
		return;
	}

	Super::NotifyPlayerReadyForGameFlow(PlayerController);
	if (!AreAugmentDraftDependenciesReady())
	{
		return;
	}

	MatchStartManager->NotifyPlayerLoaded();
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
	if (IsValid(MatchStartManager))
	{
		MatchStartManager->RequestCombatClass(PlayerController, RequestedClass);
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

void ADominationGameMode::RespawnEliminatedPlayer(AController* EliminatedController)
{
	APlayerController* PlayerController = Cast<APlayerController>(EliminatedController);
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	const ADominationGameState* DominationGameState = GetGameState<ADominationGameState>();
	if (!IsValid(PlayerController)
		|| !IsValid(PlayerState)
		|| !IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::InProgress)
	{
		return;
	}

	if (!RestartPlayerAtObjective(
		PlayerController,
		DominationGameState->GetActivePointId()))
	{
		UE_LOG(
			LogDominationGameMode,
			Error,
			TEXT("Could not resolve a Domination respawn for team %d at active point %d."),
			static_cast<uint8>(PlayerState->GetTeam()),
			static_cast<uint8>(DominationGameState->GetActivePointId()));
		return;
	}

	APawn* SpawnedPawn = PlayerController->GetPawn();
	PlayerState->ServerRespawnReset();
	RespawnManager->BeginRespawnProtection(PlayerState);
	if (!IsValid(LoadoutManager)
		|| !LoadoutManager->ApplyCombatClassLoadout(PlayerState, SpawnedPawn))
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Failed to restore the Domination class loadout for %s."),
			*GetNameSafe(PlayerState));
		return;
	}

	ServeNextAugmentReward(PlayerState);
}

bool ADominationGameMode::HasValidCombatClassConfiguration() const
{
	const auto HasPawnClass = [this](const EDominationCombatClass CombatClass)
	{
		const TSubclassOf<AShooterCharacter>* PawnClass =
			CombatClassPawnClasses.Find(CombatClass);
		return PawnClass != nullptr && PawnClass->Get() != nullptr;
	};

	return HasPawnClass(EDominationCombatClass::Trooper)
		&& HasPawnClass(EDominationCombatClass::Ranger)
		&& HasPawnClass(EDominationCombatClass::Vanguard)
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
	if (!IsValid(ObjectiveManager) || !ObjectiveManager->ActivateInitialPoint())
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination failed to activate the center capture point."));
		return;
	}

	GrantAugmentRewardsForRound(1);

	TArray<ADominationPlayerState*> InitialPlayerStates;
	GetInitialParticipantPlayerStates(InitialPlayerStates);
	for (ADominationPlayerState* PlayerState : InitialPlayerStates)
	{
		ServeNextAugmentReward(PlayerState);
	}

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

	if (AShooterPlayerState* PlayerState = NewPlayer->GetPlayerState<AShooterPlayerState>())
	{
		AugmentDraftManager->ResetPlayerForNewMatch(PlayerState);
	}
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
	const TSubclassOf<AShooterCharacter>* PawnClass = CombatClassPawnClasses.Find(CombatClass);
	if (PawnClass == nullptr || PawnClass->Get() == nullptr)
	{
		UE_LOG(
			LogDominationGameMode,
			Error,
			TEXT("No Pawn class is configured for Domination combat class %d."),
			static_cast<uint8>(CombatClass));
		return nullptr;
	}

	return PawnClass->Get();
}
