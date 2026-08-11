#include "GameFlow/Bomb/BombGameMode.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFlow/Bomb/BombGameState.h"
#include "GameFlow/Bomb/BombPlayerController.h"
#include "GameFlow/Bomb/BombPlayerStart.h"
#include "GameFlow/Bomb/DataJack.h"
#include "GameFlow/Bomb/DataJackSiteVolume.h"
#include "GameFlow/Bomb/DataJackSpawnPoint.h"
#include "GameFlow/Common/BaseTeamPlayerState.h"
#include "GameFlow/Round/BaseRoundGameState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogBombGameMode, Log, All);

ABombGameMode::ABombGameMode()
{
	GameStateClass = ABombGameState::StaticClass();
	PlayerControllerClass = ABombPlayerController::StaticClass();
	MinimumPlayersToStart = 2;
	RoundDuration = 100.0f;
	SideSwitchRound = 0;
	RoundsToWin = 2;
	MaxRounds = 3;
	StartingTeamARole = ETeamRole::Attackers;
	StartingTeamBRole = ETeamRole::Defenders;
}

void ABombGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!DataJackClass || DataJackClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogBombGameMode, Error, TEXT("%s requires a concrete Data Jack class."), *GetNameSafe(this));
	}
}

bool ABombGameMode::StartDataJackInteraction(ABombPlayerController* PlayerController)
{
	ABaseTeamPlayerState* PlayerState = nullptr;
	if (!IsDataJackRequestValid(PlayerController, PlayerState) || !IsValid(ActiveDataJack))
	{
		return false;
	}

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	const ETeamRole PlayerRole = IsValid(RoundGameState) ? RoundGameState->GetTeamRole(PlayerState->GetTeam()) : ETeamRole::None;
	const EDataJackState ObjectiveState = ActiveDataJack->GetDataJackState();
	if (ActiveDataJack->IsInteractingPlayer(PlayerState))
	{
		return (PlayerRole == ETeamRole::Attackers && ObjectiveState == EDataJackState::Planting)
			|| (PlayerRole == ETeamRole::Defenders && ObjectiveState == EDataJackState::Defusing);
	}

	bool bInteractionStarted = false;
	if (PlayerRole == ETeamRole::Attackers
		&& ActiveDataJack->GetDataJackState() == EDataJackState::Carried
		&& ActiveDataJack->IsCarriedBy(PlayerState))
	{
		ADataJackSiteVolume* SiteVolume = FindPlantSiteForPawn(PlayerController->GetPawn());
		bInteractionStarted = IsValid(SiteVolume) && ActiveDataJack->BeginPlanting(PlayerController, PlayerState, SiteVolume);
	}
	else if (PlayerRole == ETeamRole::Defenders && ActiveDataJack->GetDataJackState() == EDataJackState::Planted)
	{
		bInteractionStarted = ActiveDataJack->BeginDefusing(PlayerController, PlayerState);
	}

	return bInteractionStarted;
}

void ABombGameMode::StopDataJackInteraction(ABombPlayerController* PlayerController)
{
	if (HasAuthority() && IsValid(ActiveDataJack) && IsValid(PlayerController))
	{
		ActiveDataJack->CancelInteraction(PlayerController);
	}
}

bool ABombGameMode::StartDataJackDrop(ABombPlayerController* PlayerController)
{
	ABaseTeamPlayerState* PlayerState = nullptr;
	return IsDataJackRequestValid(PlayerController, PlayerState)
		&& IsValid(ActiveDataJack)
		&& ActiveDataJack->BeginDropHold(PlayerController, PlayerState);
}

void ABombGameMode::StopDataJackDrop(ABombPlayerController* PlayerController)
{
	if (HasAuthority() && IsValid(ActiveDataJack) && IsValid(PlayerController))
	{
		ActiveDataJack->CancelDropHold(PlayerController);
	}
}

bool ABombGameMode::TryPickupDataJack(APawn* PlayerPawn)
{
	if (!HasAuthority() || !IsValid(PlayerPawn) || !IsValid(ActiveDataJack))
	{
		return false;
	}

	AController* PlayerController = PlayerPawn->GetController();
	ABaseTeamPlayerState* PlayerState = IsValid(PlayerController) ? PlayerController->GetPlayerState<ABaseTeamPlayerState>() : nullptr;
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(PlayerState)
		|| !PlayerState->IsAlive()
		|| !IsValid(RoundGameState)
		|| RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundInProgress
		|| RoundGameState->GetTeamRole(PlayerState->GetTeam()) != ETeamRole::Attackers)
	{
		return false;
	}

	return ActiveDataJack->TryPickup(PlayerPawn, PlayerState);
}

void ABombGameMode::NotifyDataJackInteractionStopped(APlayerController* PlayerController)
{
	if (ABombPlayerController* BombPlayerController = Cast<ABombPlayerController>(PlayerController))
	{
		BombPlayerController->SetObjectiveInteractionActive(false);
	}
}

void ABombGameMode::NotifyDataJackPlanted(ADataJack* DataJack)
{
	if (!HasAuthority() || DataJack != ActiveDataJack)
	{
		return;
	}

	FreezeRoundTimer();
}

void ABombGameMode::NotifyDataJackDetonated(ADataJack* DataJack)
{
	if (!HasAuthority() || DataJack != ActiveDataJack)
	{
		return;
	}

	const EGameTeam Attackers = GetTeamForRole(ETeamRole::Attackers);
	if (Attackers != EGameTeam::None)
	{
		EndRound(Attackers, ERoundEndReason::ObjectiveCompleted);
	}
}

void ABombGameMode::NotifyDataJackDefused(ADataJack* DataJack)
{
	if (!HasAuthority() || DataJack != ActiveDataJack)
	{
		return;
	}

	const EGameTeam Defenders = GetTeamForRole(ETeamRole::Defenders);
	if (Defenders != EGameTeam::None)
	{
		EndRound(Defenders, ERoundEndReason::ObjectiveCompleted);
	}
}

AActor* ABombGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Player))
	{
		EnsurePlayerTeamAssigned(PlayerController);
	}

	const ABaseTeamPlayerState* TeamPlayerState = IsValid(Player) ? Player->GetPlayerState<ABaseTeamPlayerState>() : nullptr;
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!IsValid(TeamPlayerState) || !IsValid(RoundGameState))
	{
		UE_LOG(LogBombGameMode, Error, TEXT("Cannot choose a bomb-mode spawn because the team or round state is invalid for %s."), *GetNameSafe(Player));
		return nullptr;
	}

	const ETeamRole SpawnRole = RoundGameState->GetTeamRole(TeamPlayerState->GetTeam());
	if (SpawnRole == ETeamRole::None)
	{
		UE_LOG(LogBombGameMode, Error, TEXT("Cannot choose a bomb-mode spawn because %s has no active team role."), *GetNameSafe(Player));
		return nullptr;
	}

	TArray<ABombPlayerStart*> MatchingStarts;
	for (TActorIterator<ABombPlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ABombPlayerStart* PlayerStart = *Iterator;
		if (IsValid(PlayerStart) && PlayerStart->GetSpawnRole() == SpawnRole)
		{
			MatchingStarts.Add(PlayerStart);
		}
	}

	if (MatchingStarts.Num() == 0)
	{
		UE_LOG(LogBombGameMode, Error, TEXT("No BombPlayerStart is configured for role %d."), static_cast<int32>(SpawnRole));
		return nullptr;
	}

	TArray<ABombPlayerStart*> ClearStarts;
	for (ABombPlayerStart* PlayerStart : MatchingStarts)
	{
		if (IsPlayerStartClear(PlayerStart))
		{
			ClearStarts.Add(PlayerStart);
		}
	}

	const TArray<ABombPlayerStart*>& CandidateStarts = ClearStarts.Num() == 0 ? MatchingStarts : ClearStarts;
	return CandidateStarts[FMath::RandHelper(CandidateStarts.Num())];
}

bool ABombGameMode::PrepareRound()
{
	return Super::PrepareRound() && SpawnDataJackForRound();
}

void ABombGameMode::HandlePreparedRoundAborted()
{
	Super::HandlePreparedRoundAborted();
	GetWorldTimerManager().ClearTimer(EliminationResolutionTimerHandle);
	DestroyDataJackForRound();
}

void ABombGameMode::HandleGameFlowReset()
{
	Super::HandleGameFlowReset();
	GetWorldTimerManager().ClearTimer(EliminationResolutionTimerHandle);
	DestroyDataJackForRound();
}

void ABombGameMode::HandleRoundStarted()
{
	Super::HandleRoundStarted();

	if (IsValid(ActiveDataJack))
	{
		ActiveDataJack->ActivateForRound();
	}
}

void ABombGameMode::HandleRoundEnded(const EGameTeam WinningTeam, const ERoundEndReason EndReason)
{
	Super::HandleRoundEnded(WinningTeam, EndReason);
	GetWorldTimerManager().ClearTimer(EliminationResolutionTimerHandle);

	if (IsValid(ActiveDataJack))
	{
		ActiveDataJack->DeactivateForRound();
	}
}

void ABombGameMode::HandlePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState)
{
	Super::HandlePlayerEliminated(EliminatedPlayerState);

	if (IsValid(ActiveDataJack))
	{
		ActiveDataJack->HandlePlayerEliminated(EliminatedPlayerState);
	}

	ScheduleEliminationResolution();
}

void ABombGameMode::HandleRoundTimeExpired()
{
	if (IsValid(ActiveDataJack)
		&& (ActiveDataJack->GetDataJackState() == EDataJackState::Planted || ActiveDataJack->GetDataJackState() == EDataJackState::Defusing))
	{
		return;
	}

	if (TryResolveEliminationOutcome())
	{
		return;
	}

	const EGameTeam Defenders = GetTeamForRole(ETeamRole::Defenders);
	if (Defenders != EGameTeam::None)
	{
		EndRound(Defenders, ERoundEndReason::RoundTimeExpired);
	}
}

bool ABombGameMode::HasEnoughPlayersToStart() const
{
	return Super::HasEnoughPlayersToStart()
		&& GetTeamPlayerCount(EGameTeam::TeamA) > 0
		&& GetTeamPlayerCount(EGameTeam::TeamB) > 0;
}

bool ABombGameMode::IsGameFlowConfigurationValid() const
{
	const bool bHasValidRoles =
		(StartingTeamARole == ETeamRole::Attackers && StartingTeamBRole == ETeamRole::Defenders)
		|| (StartingTeamARole == ETeamRole::Defenders && StartingTeamBRole == ETeamRole::Attackers);

	const int32 MinimumRoundsForWinningScore = (RoundsToWin * 2) - 1;

	return Super::IsGameFlowConfigurationValid()
		&& PlayerControllerClass->IsChildOf(ABombPlayerController::StaticClass())
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
		GetWorldTimerManager().SetTimerForNextTick(this, &ABombGameMode::ResolveEliminationOutcome);
		return;
	}

	GetWorldTimerManager().SetTimer(
		EliminationResolutionTimerHandle,
		this,
		&ABombGameMode::ResolveEliminationOutcome,
		MutualEliminationGracePeriod,
		false);
}

void ABombGameMode::ResolveEliminationOutcome()
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
	const EDataJackState ObjectiveState = IsValid(ActiveDataJack) ? ActiveDataJack->GetDataJackState() : EDataJackState::Idle;
	const bool bObjectiveIsPlanted = ObjectiveState == EDataJackState::Planted || ObjectiveState == EDataJackState::Defusing;

	if (AliveAttackers == 0 && AliveDefenders == 0)
	{
		EndRound(Defenders, ERoundEndReason::MutualElimination);
		return true;
	}

	if (AliveDefenders == 0)
	{
		EndRound(Attackers, ERoundEndReason::TeamEliminated);
		return true;
	}

	if (AliveAttackers == 0 && !bObjectiveIsPlanted)
	{
		EndRound(Defenders, ERoundEndReason::TeamEliminated);
		return true;
	}

	return false;
}

bool ABombGameMode::SpawnDataJackForRound()
{
	DestroyDataJackForRound();
	if (!ValidateDataJackMapConfiguration())
	{
		return false;
	}

	ADataJackSpawnPoint* SpawnPoint = nullptr;
	for (TActorIterator<ADataJackSpawnPoint> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		SpawnPoint = *Iterator;
		break;
	}

	if (!IsValid(SpawnPoint))
	{
		UE_LOG(LogBombGameMode, Error, TEXT("Failed to find the configured Data Jack spawn point."));
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveDataJack = GetWorld()->SpawnActor<ADataJack>(DataJackClass, SpawnPoint->GetActorTransform(), SpawnParameters);
	if (!IsValid(ActiveDataJack))
	{
		UE_LOG(LogBombGameMode, Error, TEXT("Failed to spawn the Data Jack actor."));
		return false;
	}

	ActiveDataJack->ConfigureForMode(
		PlantDuration,
		CountdownDuration,
		DefuseDuration,
		DropHoldDuration,
		InteractionValidationInterval);
	ActiveDataJack->InitializeForRound(SpawnPoint->GetActorTransform());

	ABombGameState* BombGameState = GetGameState<ABombGameState>();
	if (!IsValid(BombGameState))
	{
		UE_LOG(LogBombGameMode, Error, TEXT("Failed to publish the active Data Jack because BombGameState is unavailable."));
		ActiveDataJack->Destroy();
		ActiveDataJack = nullptr;
		return false;
	}

	BombGameState->SetActiveDataJack(ActiveDataJack);
	return true;
}

void ABombGameMode::DestroyDataJackForRound()
{
	if (ABombGameState* BombGameState = GetGameState<ABombGameState>())
	{
		BombGameState->SetActiveDataJack(nullptr);
	}

	if (IsValid(ActiveDataJack))
	{
		ActiveDataJack->DeactivateForRound();
		ActiveDataJack->Destroy();
	}

	ActiveDataJack = nullptr;
}

bool ABombGameMode::ValidateDataJackMapConfiguration() const
{
	int32 DataJackSpawnPointCount = 0;
	for (TActorIterator<ADataJackSpawnPoint> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (IsValid(*Iterator))
		{
			++DataJackSpawnPointCount;
		}
	}

	if (DataJackSpawnPointCount != 1)
	{
		UE_LOG(
			LogBombGameMode,
			Error,
			TEXT("Bomb mode requires exactly one attacker-side DataJackSpawnPoint, but found %d."),
			DataJackSpawnPointCount);
		return false;
	}

	int32 SiteCount = 0;
	TSet<FName> SiteIds;
	for (TActorIterator<ADataJackSiteVolume> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		const ADataJackSiteVolume* SiteVolume = *Iterator;
		if (!IsValid(SiteVolume))
		{
			continue;
		}

		const FName SiteId = SiteVolume->GetSiteId();
		if (SiteId.IsNone() || SiteIds.Contains(SiteId))
		{
			UE_LOG(LogBombGameMode, Error, TEXT("Every DataJackSiteVolume requires a unique, non-empty SiteId."));
			return false;
		}

		SiteIds.Add(SiteId);
		++SiteCount;
	}

	if (SiteCount == 0)
	{
		UE_LOG(LogBombGameMode, Error, TEXT("Bomb mode requires at least one DataJackSiteVolume."));
		return false;
	}

	return true;
}

ADataJackSiteVolume* ABombGameMode::FindPlantSiteForPawn(const APawn* PlayerPawn) const
{
	if (!IsValid(PlayerPawn))
	{
		return nullptr;
	}

	for (TActorIterator<ADataJackSiteVolume> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ADataJackSiteVolume* SiteVolume = *Iterator;
		if (IsValid(SiteVolume) && SiteVolume->ContainsPawn(PlayerPawn))
		{
			return SiteVolume;
		}
	}

	return nullptr;
}

bool ABombGameMode::IsDataJackRequestValid(
	const ABombPlayerController* PlayerController,
	ABaseTeamPlayerState*& OutPlayerState) const
{
	OutPlayerState = nullptr;
	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (!HasAuthority()
		|| !IsValid(PlayerController)
		|| !PlayerController->IsRoundInputEnabled()
		|| !IsValid(RoundGameState)
		|| RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundInProgress
		|| !IsValid(PlayerController->GetPawn()))
	{
		return false;
	}

	OutPlayerState = PlayerController->GetPlayerState<ABaseTeamPlayerState>();
	return IsValid(OutPlayerState) && OutPlayerState->IsAlive() && OutPlayerState->GetTeam() != EGameTeam::None;
}
