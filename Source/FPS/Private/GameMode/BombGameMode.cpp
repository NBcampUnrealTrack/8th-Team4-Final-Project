#include "GameMode/BombGameMode.h"

#include "EngineUtils.h"
#include "GameMode/State/BombGameState.h"
#include "GameMode/Manager/BombObjectiveManager.h"
#include "Player/BombPlayerController.h"
#include "Actor/BombPlayerStart.h"
#include "GameMode/Rules/BombRoundRules.h"
#include "Actor/DataJack.h"
#include "Player/BaseTeamPlayerState.h"
#include "GameMode/Manager/TeamPlayerManager.h"
#include "GameMode/State/BaseRoundGameState.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogBombGameMode, Log, All);

ABombGameMode::ABombGameMode()
{
	GameStateClass = ABombGameState::StaticClass();
	PlayerControllerClass = ABombPlayerController::StaticClass();

	// Listen Server PIE 최소값이다. 실제 4대4 정원과 Ready 검증은 로비 계층에서 확정한다.
	MinimumPlayersToStart = 2;
	RoundDuration = 160.0f;
	SideSwitchRound = 7;
	RoundsToWin = 7;
	MaxRounds = 13;
	StartingTeamARole = ETeamRole::Attackers;
	StartingTeamBRole = ETeamRole::Defenders;

	BombObjectiveManager = CreateDefaultSubobject<UBombObjectiveManager>(TEXT("BombObjectiveManager"));
}

void ABombGameMode::NotifyDataJackPlanted(ADataJack* DataJack)
{
	const UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager();
	if (!HasAuthority() || !IsValid(ObjectiveManager) || !ObjectiveManager->IsActiveDataJack(DataJack))
	{
		return;
	}

	// 설치 보상은 라운드 결과와 무관하게 유지되며, 실패 사유는 래퍼가 로깅한다.
	RecordRoleObjectiveReward(ETeamRole::Attackers);
	FreezeRoundTimer();
}

void ABombGameMode::NotifyDataJackDetonated(ADataJack* DataJack)
{
	const UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager();
	if (!HasAuthority() || !IsValid(ObjectiveManager) || !ObjectiveManager->IsActiveDataJack(DataJack))
	{
		return;
	}

	// 팀 목표 보상은 설치 완료 시 이미 기록되므로 폭발에서는 추가하지 않는다.
	const EGameTeam Attackers = GetTeamForRole(ETeamRole::Attackers);
	if (Attackers != EGameTeam::None)
	{
		EndRound(Attackers, ERoundEndReason::ObjectiveCompleted);
	}
}

void ABombGameMode::NotifyDataJackDefused(ADataJack* DataJack)
{
	const UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager();
	if (!HasAuthority() || !IsValid(ObjectiveManager) || !ObjectiveManager->IsActiveDataJack(DataJack))
	{
		return;
	}

	const EGameTeam Defenders = GetTeamForRole(ETeamRole::Defenders);
	if (Defenders == EGameTeam::None)
	{
		UE_LOG(LogBombGameMode, Error, TEXT("Cannot resolve the defusing team."));
		return;
	}

	// 보상은 라운드가 닫히기 전에 예약해야 정산에 포함된다.
	RecordRoleObjectiveReward(ETeamRole::Defenders);
	EndRound(Defenders, ERoundEndReason::ObjectiveCompleted);
}

void ABombGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (IsValid(BombObjectiveManager))
	{
		BombObjectiveManager->Initialize(this);
	}
}

void ABombGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	GetWorldTimerManager().ClearTimer(EliminationResolutionTimerHandle);
	if (UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager())
	{
		ObjectiveManager->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}

AActor* ABombGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const ABaseTeamPlayerState* TeamPlayerState = IsValid(Player)
		? Player->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	if (!IsValid(TeamPlayerState))
	{
		UE_LOG(LogBombGameMode, Error,
			TEXT("Cannot choose a bomb-mode spawn because team state is invalid for %s."),
			*GetNameSafe(Player));
		return nullptr;
	}

	// 다음 라운드 역할은 스폰 준비에만 먼저 사용하고, 준비 완료 후 GameState에 공개한다.
	const ETeamRole SpawnRole = GetSpawnRoleForTeam(TeamPlayerState->GetTeam());
	if (SpawnRole == ETeamRole::None)
	{
		UE_LOG(LogBombGameMode, Error, TEXT("%s has no active team role."), *GetNameSafe(Player));
		return nullptr;
	}

	TArray<AActor*> MatchingStarts;
	for (TActorIterator<ABombPlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ABombPlayerStart* PlayerStart = *Iterator;
		if (IsValid(PlayerStart) && PlayerStart->GetSpawnRole() == SpawnRole)
		{
			MatchingStarts.Add(PlayerStart);
		}
	}

	UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	return IsValid(PlayerManager)
		? PlayerManager->SelectAvailablePlayerStart(MatchingStarts, TEXT("BombPlayerStart"))
		: nullptr;
}

void ABombGameMode::ResetModeState()
{
	GetWorldTimerManager().ClearTimer(EliminationResolutionTimerHandle);
	if (UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager())
	{
		ObjectiveManager->ResetObjective();
	}
}

EPlayerStartReservationResult ABombGameMode::ReserveModePlayerStarts(const FRoundContext& RoundContext)
{
	UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	if (!IsValid(PlayerManager) || !RoundContext.IsValid())
	{
		return EPlayerStartReservationResult::InvalidConfiguration;
	}

	FPlayerStartGroup AttackerStarts;
	AttackerStarts.Description = TEXT("attacker BombPlayerStart group");
	AttackerStarts.RequiredCount = GetTeamPlayerCount(RoundContext.GetTeamForRole(ETeamRole::Attackers));

	FPlayerStartGroup DefenderStarts;
	DefenderStarts.Description = TEXT("defender BombPlayerStart group");
	DefenderStarts.RequiredCount = GetTeamPlayerCount(RoundContext.GetTeamForRole(ETeamRole::Defenders));

	for (TActorIterator<ABombPlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ABombPlayerStart* PlayerStart = *Iterator;
		if (!IsValid(PlayerStart))
		{
			continue;
		}

		switch (PlayerStart->GetSpawnRole())
		{
		case ETeamRole::Attackers:
			AttackerStarts.PlayerStarts.Add(PlayerStart);
			break;
		case ETeamRole::Defenders:
			DefenderStarts.PlayerStarts.Add(PlayerStart);
			break;
		default:
			break;
		}
	}

	TArray<FPlayerStartGroup> StartGroups;
	StartGroups.Reserve(2);
	StartGroups.Add(MoveTemp(AttackerStarts));
	StartGroups.Add(MoveTemp(DefenderStarts));
	return PlayerManager->ReservePlayerStarts(StartGroups);
}

bool ABombGameMode::PrepareModeRound(const FRoundContext&)
{
	UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager();
	if (!IsValid(ObjectiveManager))
	{
		return false;
	}

	FBombObjectiveConfig ObjectiveConfig;
	ObjectiveConfig.DataJackClass = DataJackClass;
	ObjectiveConfig.PlantDuration = PlantDuration;
	ObjectiveConfig.CountdownDuration = CountdownDuration;
	ObjectiveConfig.DefuseDuration = DefuseDuration;
	ObjectiveConfig.DropHoldDuration = DropHoldDuration;
	ObjectiveConfig.InteractionValidationInterval = InteractionValidationInterval;
	return ObjectiveManager->PrepareForRound(ObjectiveConfig);
}

bool ABombGameMode::ActivateModeRound()
{
	UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager();
	return IsValid(ObjectiveManager) && ObjectiveManager->ActivateForRound();
}

void ABombGameMode::DeactivateModeRound(const EGameTeam, const ERoundEndReason)
{
	GetWorldTimerManager().ClearTimer(EliminationResolutionTimerHandle);
	if (UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager())
	{
		ObjectiveManager->DeactivateForRound();
	}
}

void ABombGameMode::HandleModePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState)
{
	if (UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager())
	{
		ObjectiveManager->HandlePlayerEliminated(EliminatedPlayerState);
	}

	ScheduleEliminationResolution();
}

void ABombGameMode::ResolveModeRoundTimeExpired()
{
	const UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager();
	const bool bObjectiveIsPlanted = IsValid(ObjectiveManager) && ObjectiveManager->IsPlantedOrDefusing();
	const EGameTeam Attackers = GetTeamForRole(ETeamRole::Attackers);
	const EGameTeam Defenders = GetTeamForRole(ETeamRole::Defenders);
	const FBombRoundResolution Resolution = BombRoundRules::ResolveTimeExpired(
		GetAlivePlayerCount(Attackers),
		GetAlivePlayerCount(Defenders),
		bObjectiveIsPlanted,
		Attackers,
		Defenders);
	if (Resolution.ShouldEndRound())
	{
		EndRound(Resolution.WinningTeam, Resolution.EndReason);
	}
}

EGameTeam ABombGameMode::ResolveModeMatchWinnerAfterPlayerDeparture(const EGameTeam DepartingTeam) const
{
	if (DepartingTeam == EGameTeam::None || GetTeamPlayerCount(DepartingTeam) > 1)
	{
		return EGameTeam::None;
	}

	const EGameTeam OpposingTeam = DepartingTeam == EGameTeam::TeamA
		? EGameTeam::TeamB
		: EGameTeam::TeamA;
	return GetTeamPlayerCount(OpposingTeam) > 0
		? OpposingTeam
		: EGameTeam::None;
}

bool ABombGameMode::HasModeRequiredPlayers() const
{
	return GetTeamPlayerCount(EGameTeam::TeamA) > 0
		&& GetTeamPlayerCount(EGameTeam::TeamB) > 0;
}

bool ABombGameMode::IsModeConfigurationValid() const
{
	const bool bHasValidRoles =
		(StartingTeamARole == ETeamRole::Attackers && StartingTeamBRole == ETeamRole::Defenders)
		|| (StartingTeamARole == ETeamRole::Defenders && StartingTeamBRole == ETeamRole::Attackers);
	const int32 MinimumRoundsForWinningScore = (RoundsToWin * 2) - 1;

	return PlayerControllerClass != nullptr
		&& PlayerControllerClass->IsChildOf(ABombPlayerController::StaticClass())
		&& GameStateClass != nullptr
		&& GameStateClass->IsChildOf(ABombGameState::StaticClass())
		&& DataJackClass != nullptr
		&& !DataJackClass->HasAnyClassFlags(CLASS_Abstract)
		&& MinimumPlayersToStart >= 2
		&& RoundDuration > 0.0f
		&& MaxRounds > 0
		&& (SideSwitchRound == 0 || (SideSwitchRound > 1 && SideSwitchRound <= MaxRounds))
		&& RoundsToWin > 0
		&& MaxRounds >= MinimumRoundsForWinningScore
		&& PlantDuration > 0.0f
		&& CountdownDuration > 0.0f
		&& DefuseDuration > 0.0f
		&& DropHoldDuration > 0.0f
		&& InteractionValidationInterval >= 0.02f
		&& MutualEliminationGracePeriod >= 0.0f
		&& bHasValidRoles;
}

void ABombGameMode::ScheduleEliminationResolution()
{
	GetWorldTimerManager().ClearTimer(EliminationResolutionTimerHandle);

	if (MutualEliminationGracePeriod <= 0.0f)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ABombGameMode::HandleEliminationGraceExpired);
		return;
	}

	GetWorldTimerManager().SetTimer(
		EliminationResolutionTimerHandle,
		this,
		&ABombGameMode::HandleEliminationGraceExpired,
		MutualEliminationGracePeriod,
		false);
}

void ABombGameMode::HandleEliminationGraceExpired()
{
	TryResolveEliminationOutcome();
}

bool ABombGameMode::TryResolveEliminationOutcome()
{
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(RoundGameState) || RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundInProgress)
	{
		return false;
	}

	const EGameTeam Attackers = GetTeamForRole(ETeamRole::Attackers);
	const EGameTeam Defenders = GetTeamForRole(ETeamRole::Defenders);
	if (Attackers == EGameTeam::None || Defenders == EGameTeam::None)
	{
		return false;
	}

	const int32 AliveAttackers = GetAlivePlayerCount(Attackers);
	const int32 AliveDefenders = GetAlivePlayerCount(Defenders);
	const UBombObjectiveManager* ObjectiveManager = GetBombObjectiveManager();
	const bool bObjectiveIsPlanted = IsValid(ObjectiveManager) && ObjectiveManager->IsPlantedOrDefusing();

	const FBombRoundResolution Resolution = BombRoundRules::ResolveElimination(
		AliveAttackers,
		AliveDefenders,
		bObjectiveIsPlanted,
		Attackers,
		Defenders);
	if (Resolution.ShouldEndRound())
	{
		EndRound(Resolution.WinningTeam, Resolution.EndReason);
		return true;
	}

	return false;
}

UBombObjectiveManager* ABombGameMode::GetBombObjectiveManager() const
{
	return BombObjectiveManager.Get();
}
