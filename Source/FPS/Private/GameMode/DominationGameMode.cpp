#include "GameMode/DominationGameMode.h"

#include "Character/ShooterCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/DominationGameState.h"
#include "GameMode/DominationLoadoutManager.h"
#include "GameMode/DominationMatchStartManager.h"
#include "GameMode/DominationObjectiveManager.h"
#include "GameMode/DominationRespawnManager.h"
#include "GameMode/TeamPlayerManager.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"

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

	CombatClassPawnClasses.Add(EDominationCombatClass::Trooper, nullptr);
	CombatClassPawnClasses.Add(EDominationCombatClass::Ranger, nullptr);
	CombatClassPawnClasses.Add(EDominationCombatClass::Vanguard, nullptr);
}

void ADominationGameMode::BeginPlay()
{
	Super::BeginPlay();
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
		|| !RespawnManager->Initialize(this, ObjectiveManager, RespawnDelay))
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Domination match start stopped because the spawn volumes are not configured correctly."));
		return;
	}

	if (IsValid(MatchStartManager))
	{
		MatchStartManager->OnInitialPlayersSpawned.AddUObject(
			this,
			&ThisClass::HandleInitialPlayersSpawned);
		MatchStartManager->Initialize(this, RequiredPlayersPerTeam, ClassSelectionDuration);
	}
}

void ADominationGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
	MatchStartManager->NotifyPlayerLoaded();
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
	if (!IsValid(LoadoutManager)
		|| !LoadoutManager->ApplyCombatClassLoadout(PlayerState, SpawnedPawn))
	{
		UE_LOG(LogDominationGameMode, Error,
			TEXT("Failed to restore the Domination class loadout for %s."),
			*GetNameSafe(PlayerState));
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
	DominationGameState->SetDominationPhase(EDominationMatchPhase::MatchEnded);

	if (IsValid(ObjectiveManager))
	{
		ObjectiveManager->StopObjectiveFlow();
	}
	if (IsValid(RespawnManager))
	{
		RespawnManager->CancelPendingRespawns();
	}

	EndMatch();
	UE_LOG(
		LogDominationGameMode,
		Log,
		TEXT("Domination match ended. Winner: Team %d, final score: TeamA %d - TeamB %d."),
		static_cast<uint8>(WinningTeam),
		DominationGameState->GetTeamAScore(),
		DominationGameState->GetTeamBScore());
}

void ADominationGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// BaseTeamGameMode의 Super 경로는 즉시 Pawn을 생성하므로 팀 배정만 수행한다.
	if (!InitializeTeamPlayer(NewPlayer))
	{
		return;
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
