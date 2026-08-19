#include "GameMode/TeamPlayerManager.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameMode/BaseTeamGameMode.h"
#include "GameMode/BaseTeamGameState.h"
#include "Player/BaseTeamPlayerState.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogTeamPlayerManager, Log, All);

void UTeamPlayerManager::Initialize(ABaseTeamGameMode* InOwnerGameMode, const float InSpawnClearanceRadius)
{
	OwnerGameMode = InOwnerGameMode;
	SpawnClearanceRadius = FMath::Max(0.0f, InSpawnClearanceRadius);
	ClearPlayerStartReservations();
	ResetRoundTracking();
}

int32 UTeamPlayerManager::GetTeamPlayerCount(const EGameTeam Team) const
{
	const ABaseTeamGameMode* GameMode = GetOwnerGameMode();
	const ABaseTeamGameState* TeamGameState = IsValid(GameMode)
		? GameMode->GetGameState<ABaseTeamGameState>()
		: nullptr;
	if (!IsValid(TeamGameState))
	{
		return 0;
	}

	int32 PlayerCount = 0;
	for (const APlayerState* PlayerState : TeamGameState->PlayerArray)
	{
		const ABaseTeamPlayerState* TeamPlayerState = Cast<ABaseTeamPlayerState>(PlayerState);
		if (IsValid(TeamPlayerState) && TeamPlayerState->GetTeam() == Team)
		{
			++PlayerCount;
		}
	}

	return PlayerCount;
}

int32 UTeamPlayerManager::GetAlivePlayerCount(const EGameTeam Team) const
{
	int32 AlivePlayerCount = 0;

	if (!RoundParticipants.IsEmpty())
	{
		for (const FTeamRoundParticipant& Participant : RoundParticipants)
		{
			const AShooterPlayerState* PlayerState = Participant.PlayerState.Get();
			if (IsValid(PlayerState) && PlayerState->GetTeam() == Team && PlayerState->IsAlive())
			{
				++AlivePlayerCount;
			}
		}
		return AlivePlayerCount;
	}

	const ABaseTeamGameMode* GameMode = GetOwnerGameMode();
	const ABaseTeamGameState* TeamGameState = IsValid(GameMode)
		? GameMode->GetGameState<ABaseTeamGameState>()
		: nullptr;
	if (!IsValid(TeamGameState))
	{
		return 0;
	}

	for (const APlayerState* PlayerState : TeamGameState->PlayerArray)
	{
		const ABaseTeamPlayerState* TeamPlayerState = Cast<ABaseTeamPlayerState>(PlayerState);
		if (IsValid(TeamPlayerState) && TeamPlayerState->GetTeam() == Team && TeamPlayerState->IsAlive())
		{
			++AlivePlayerCount;
		}
	}

	return AlivePlayerCount;
}

bool UTeamPlayerManager::AssignTeamIfNeeded(APlayerController* PlayerController, const EGameTeam TeamToAssign) const
{
	if (!IsValid(PlayerController))
	{
		return false;
	}

	ABaseTeamPlayerState* TeamPlayerState = PlayerController->GetPlayerState<ABaseTeamPlayerState>();
	if (!IsValid(TeamPlayerState))
	{
		UE_LOG(LogTeamPlayerManager, Error, TEXT("%s does not use ABaseTeamPlayerState."), *GetNameSafe(PlayerController));
		return false;
	}

	if (TeamPlayerState->GetTeam() == EGameTeam::None)
	{
		if (TeamToAssign == EGameTeam::None)
		{
			UE_LOG(LogTeamPlayerManager, Error, TEXT("No valid team was selected for %s."), *GetNameSafe(PlayerController));
			return false;
		}

		TeamPlayerState->SetTeam(TeamToAssign);
		TeamPlayerState->SetAlive(false);
	}

	return true;
}

EPlayerStartReservationResult UTeamPlayerManager::ReservePlayerStarts(
	const TArray<FPlayerStartGroup>& StartGroups)
{
	ClearPlayerStartReservations();

	TArray<FPlayerStartGroup> ValidatedGroups;
	ValidatedGroups.Reserve(StartGroups.Num());
	for (const FPlayerStartGroup& StartGroup : StartGroups)
	{
		if (StartGroup.RequiredCount <= 0)
		{
			continue;
		}

		FPlayerStartGroup& ValidatedGroup = ValidatedGroups.AddDefaulted_GetRef();
		ValidatedGroup.Description = StartGroup.Description.IsEmpty()
			? TEXT("PlayerStart group")
			: StartGroup.Description;
		ValidatedGroup.RequiredCount = StartGroup.RequiredCount;

		TSet<AActor*> UniqueStarts;
		for (AActor* PlayerStart : StartGroup.PlayerStarts)
		{
			if (IsValid(PlayerStart) && !UniqueStarts.Contains(PlayerStart))
			{
				UniqueStarts.Add(PlayerStart);
				ValidatedGroup.PlayerStarts.Add(PlayerStart);
			}
		}

		if (ValidatedGroup.PlayerStarts.Num() < ValidatedGroup.RequiredCount)
		{
			UE_LOG(LogTeamPlayerManager, Error,
				TEXT("Invalid %s configuration: %d valid starts are configured, but %d are required."),
				*ValidatedGroup.Description,
				ValidatedGroup.PlayerStarts.Num(),
				ValidatedGroup.RequiredCount);
			return EPlayerStartReservationResult::InvalidConfiguration;
		}
	}

	if (ValidatedGroups.IsEmpty())
	{
		UE_LOG(LogTeamPlayerManager, Error, TEXT("No PlayerStart group requires a spawn reservation."));
		return EPlayerStartReservationResult::InvalidConfiguration;
	}

	TArray<AActor*> ConfiguredReservation;
	// 필요한 수만 조합으로 고르므로 가까이 배치된 여분 Start가 있어도 사용 가능한 구성이면 허용한다.
	if (!TryBuildStartReservation(
		ValidatedGroups,
		0,
		0,
		ValidatedGroups[0].RequiredCount,
		false,
		ConfiguredReservation))
	{
		UE_LOG(LogTeamPlayerManager, Error,
			TEXT("PlayerStart configuration cannot fit all required players with SpawnClearanceRadius %.1f. "
				"Move the starts farther apart or reduce the clearance radius."),
			SpawnClearanceRadius);
		return EPlayerStartReservationResult::InvalidConfiguration;
	}

	TArray<AActor*> ClearReservation;
	if (!TryBuildStartReservation(
		ValidatedGroups,
		0,
		0,
		ValidatedGroups[0].RequiredCount,
		true,
		ClearReservation))
	{
		UE_LOG(LogTeamPlayerManager, Warning,
			TEXT("The configured PlayerStarts are currently blocked by pawns. Round spawn will be retried."));
		return EPlayerStartReservationResult::TemporarilyBlocked;
	}

	ReservedPlayerStarts.Reserve(ClearReservation.Num());
	for (AActor* PlayerStart : ClearReservation)
	{
		ReservedPlayerStarts.Add(TWeakObjectPtr<AActor>(PlayerStart));
	}
	bHasActivePlayerStartReservation = true;
	return EPlayerStartReservationResult::Succeeded;
}

void UTeamPlayerManager::ClearPlayerStartReservations()
{
	ReservedPlayerStarts.Reset();
	bHasActivePlayerStartReservation = false;
}

AActor* UTeamPlayerManager::SelectAvailablePlayerStart(
	const TArray<AActor*>& MatchingStarts,
	const TCHAR* StartDescription)
{
	TArray<AActor*> ValidStarts;
	TArray<AActor*> ClearStarts;
	ValidStarts.Reserve(MatchingStarts.Num());
	ClearStarts.Reserve(MatchingStarts.Num());

	for (AActor* PlayerStart : MatchingStarts)
	{
		if (!IsValid(PlayerStart))
		{
			continue;
		}
		if (bHasActivePlayerStartReservation
			&& !ReservedPlayerStarts.Contains(TWeakObjectPtr<AActor>(PlayerStart)))
		{
			continue;
		}

		ValidStarts.Add(PlayerStart);
		if (IsPlayerStartClear(PlayerStart))
		{
			ClearStarts.Add(PlayerStart);
		}
	}

	if (ValidStarts.IsEmpty())
	{
		UE_LOG(LogTeamPlayerManager, Error,
			TEXT("No valid or reserved %s is available for the requested spawn group."),
			StartDescription);
		return nullptr;
	}

	if (!ClearStarts.IsEmpty())
	{
		AActor* SelectedStart = ClearStarts[FMath::RandHelper(ClearStarts.Num())];
		if (bHasActivePlayerStartReservation)
		{
			ReservedPlayerStarts.Remove(TWeakObjectPtr<AActor>(SelectedStart));
		}
		return SelectedStart;
	}

	if (SpawnClearanceRadius <= 0.0f)
	{
		return ValidStarts[FMath::RandHelper(ValidStarts.Num())];
	}

	UE_LOG(LogTeamPlayerManager, Warning,
		TEXT("No clear %s is currently available."),
		StartDescription);
	return nullptr;
}

void UTeamPlayerManager::RemovePlayerPawn(APlayerController* PlayerController) const
{
	if (!IsValid(PlayerController))
	{
		return;
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

void UTeamPlayerManager::ResetAllPlayerPawns()
{
	ClearPlayerStartReservations();

	ABaseTeamGameMode* GameMode = GetOwnerGameMode();
	UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		ResetRoundTracking();
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		RemovePlayerPawn(Iterator->Get());
	}

	ResetRoundTracking();
}

ERoundPlayerPreparationResult UTeamPlayerManager::PreparePlayersForRound()
{
	ABaseTeamGameMode* GameMode = GetOwnerGameMode();
	if (!IsValid(GameMode))
	{
		ClearPlayerStartReservations();
		return ERoundPlayerPreparationResult::InvalidState;
	}

	TArray<APlayerController*> Players;
	if (!GatherEligiblePlayers(Players))
	{
		ClearPlayerStartReservations();
		return ERoundPlayerPreparationResult::InvalidState;
	}
	const bool bUsesReservedPlayerStarts = bHasActivePlayerStartReservation;

	ResetRoundTracking();
	for (APlayerController* PlayerController : Players)
	{
		RemovePlayerPawn(PlayerController);
	}

	for (APlayerController* PlayerController : Players)
	{
		AShooterPlayerState* PlayerState = PlayerController->GetPlayerState<AShooterPlayerState>();
		if (!IsValid(PlayerState))
		{
			UE_LOG(LogTeamPlayerManager, Error,
				TEXT("%s lost its ShooterPlayerState during round preparation."),
				*GetNameSafe(PlayerController));
			CleanupFailedPreparation(Players);
			return ERoundPlayerPreparationResult::InvalidState;
		}

		GameMode->RestartPlayer(PlayerController);

		APawn* SpawnedPawn = PlayerController->GetPawn();
		if (!IsValid(SpawnedPawn))
		{
			if (bUsesReservedPlayerStarts)
			{
				UE_LOG(LogTeamPlayerManager, Warning,
					TEXT("Could not spawn %s at its reserved PlayerStart; round spawn will be retried."),
					*GetNameSafe(PlayerController));
			}
			else
			{
				UE_LOG(LogTeamPlayerManager, Error,
					TEXT("Could not spawn %s because no validated PlayerStart reservation is active."),
					*GetNameSafe(PlayerController));
			}
			CleanupFailedPreparation(Players);
			return bUsesReservedPlayerStarts
				? ERoundPlayerPreparationResult::TemporarilyBlocked
				: ERoundPlayerPreparationResult::InvalidState;
		}

		if (SpawnedPawn->GetController() != PlayerController
			|| PlayerController->GetPlayerState<AShooterPlayerState>() != PlayerState)
		{
			UE_LOG(LogTeamPlayerManager, Error,
				TEXT("%s has an invalid pawn/controller state after round spawn."),
				*GetNameSafe(PlayerController));
			CleanupFailedPreparation(Players);
			return ERoundPlayerPreparationResult::InvalidState;
		}

		PlayerState->ServerRespawnReset();
		if (!PlayerState->IsAlive())
		{
			UE_LOG(LogTeamPlayerManager, Error,
				TEXT("%s remained dead after ServerRespawnReset()."),
				*GetNameSafe(PlayerState));
			CleanupFailedPreparation(Players);
			return ERoundPlayerPreparationResult::InvalidState;
		}
	}

	RoundParticipants.Reserve(Players.Num());
	for (APlayerController* PlayerController : Players)
	{
		FTeamRoundParticipant& Participant = RoundParticipants.AddDefaulted_GetRef();
		Participant.Controller = PlayerController;
		Participant.PlayerState = PlayerController->GetPlayerState<AShooterPlayerState>();
	}

	if (!AreRoundParticipantsReady())
	{
		CleanupFailedPreparation(Players);
		return ERoundPlayerPreparationResult::InvalidState;
	}

	ClearPlayerStartReservations();
	return ERoundPlayerPreparationResult::Succeeded;
}

bool UTeamPlayerManager::AreRoundParticipantsReady() const
{
	if (RoundParticipants.IsEmpty())
	{
		return false;
	}

	for (const FTeamRoundParticipant& Participant : RoundParticipants)
	{
		APlayerController* PlayerController = Participant.Controller.Get();
		AShooterPlayerState* PlayerState = Participant.PlayerState.Get();
		if (!IsValid(PlayerController)
			|| !IsValid(PlayerState)
			|| PlayerState->GetTeam() == EGameTeam::None
			|| !PlayerState->IsAlive()
			|| EliminatedPlayersThisRound.Contains(TWeakObjectPtr<ABaseTeamPlayerState>(PlayerState))
			|| !IsValid(PlayerController->GetPawn())
			|| PlayerController->GetPlayerState<AShooterPlayerState>() != PlayerState
			|| PlayerController->GetPawn()->GetController() != PlayerController)
		{
			return false;
		}
	}

	return true;
}

bool UTeamPlayerManager::IsRoundParticipant(const ABaseTeamPlayerState* PlayerState) const
{
	if (!IsValid(PlayerState))
	{
		return false;
	}

	for (const FTeamRoundParticipant& Participant : RoundParticipants)
	{
		if (Participant.PlayerState.Get() == PlayerState)
		{
			return true;
		}
	}

	return false;
}

void UTeamPlayerManager::GetRoundParticipants(TArray<AShooterPlayerState*>& OutParticipants) const
{
	OutParticipants.Reset();
	OutParticipants.Reserve(RoundParticipants.Num());

	for (const FTeamRoundParticipant& Participant : RoundParticipants)
	{
		if (AShooterPlayerState* PlayerState = Participant.PlayerState.Get())
		{
			OutParticipants.Add(PlayerState);
		}
	}
}

void UTeamPlayerManager::GetRoundParticipantControllers(TArray<APlayerController*>& OutControllers) const
{
	OutControllers.Reset();
	OutControllers.Reserve(RoundParticipants.Num());

	for (const FTeamRoundParticipant& Participant : RoundParticipants)
	{
		if (APlayerController* PlayerController = Participant.Controller.Get())
		{
			OutControllers.Add(PlayerController);
		}
	}
}

bool UTeamPlayerManager::TryMarkPlayerEliminated(
	AController* EliminatedController,
	ABaseTeamPlayerState*& OutPlayerState)
{
	OutPlayerState = IsValid(EliminatedController)
		? EliminatedController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	if (!IsRoundParticipant(OutPlayerState))
	{
		return false;
	}

	const TWeakObjectPtr<ABaseTeamPlayerState> PlayerKey(OutPlayerState);
	if (EliminatedPlayersThisRound.Contains(PlayerKey))
	{
		return false;
	}

	EliminatedPlayersThisRound.Add(PlayerKey);
	OutPlayerState->SetAlive(false);
	return true;
}

void UTeamPlayerManager::ResetRoundTracking()
{
	RoundParticipants.Reset();
	EliminatedPlayersThisRound.Reset();
}

bool UTeamPlayerManager::GatherEligiblePlayers(TArray<APlayerController*>& OutPlayers) const
{
	OutPlayers.Reset();

	const ABaseTeamGameMode* GameMode = GetOwnerGameMode();
	UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return false;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (!IsValid(PlayerController))
		{
			continue;
		}

		const AShooterPlayerState* PlayerState = PlayerController->GetPlayerState<AShooterPlayerState>();
		if (!IsValid(PlayerState) || PlayerState->GetTeam() == EGameTeam::None)
		{
			UE_LOG(LogTeamPlayerManager, Error,
				TEXT("%s cannot enter the round because its ShooterPlayerState or team assignment is invalid."),
				*GetNameSafe(PlayerController));
			return false;
		}

		OutPlayers.Add(PlayerController);
	}

	return !OutPlayers.IsEmpty();
}

void UTeamPlayerManager::CleanupFailedPreparation(const TArray<APlayerController*>& AttemptedPlayers)
{
	for (APlayerController* PlayerController : AttemptedPlayers)
	{
		RemovePlayerPawn(PlayerController);
	}
	ClearPlayerStartReservations();
	ResetRoundTracking();
}

bool UTeamPlayerManager::TryBuildStartReservation(
	const TArray<FPlayerStartGroup>& StartGroups,
	const int32 GroupIndex,
	const int32 CandidateIndex,
	const int32 RemainingInGroup,
	const bool bRequireClearStarts,
	TArray<AActor*>& InOutSelectedStarts) const
{
	if (!StartGroups.IsValidIndex(GroupIndex))
	{
		return true;
	}

	if (RemainingInGroup <= 0)
	{
		const int32 NextGroupIndex = GroupIndex + 1;
		return !StartGroups.IsValidIndex(NextGroupIndex)
			|| TryBuildStartReservation(
				StartGroups,
				NextGroupIndex,
				0,
				StartGroups[NextGroupIndex].RequiredCount,
				bRequireClearStarts,
				InOutSelectedStarts);
	}

	const FPlayerStartGroup& StartGroup = StartGroups[GroupIndex];
	if (StartGroup.PlayerStarts.Num() - CandidateIndex < RemainingInGroup)
	{
		return false;
	}

	for (int32 Index = CandidateIndex; Index < StartGroup.PlayerStarts.Num(); ++Index)
	{
		AActor* Candidate = StartGroup.PlayerStarts[Index];
		if (!IsValid(Candidate)
			|| (bRequireClearStarts && !IsPlayerStartClear(Candidate))
			|| !IsCompatibleWithSelectedStarts(Candidate, InOutSelectedStarts))
		{
			continue;
		}

		InOutSelectedStarts.Add(Candidate);
		if (TryBuildStartReservation(
			StartGroups,
			GroupIndex,
			Index + 1,
			RemainingInGroup - 1,
			bRequireClearStarts,
			InOutSelectedStarts))
		{
			return true;
		}
		InOutSelectedStarts.Pop();
	}

	return false;
}

bool UTeamPlayerManager::IsCompatibleWithSelectedStarts(
	const AActor* Candidate,
	const TArray<AActor*>& SelectedStarts) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	const float ClearanceRadiusSquared = FMath::Square(SpawnClearanceRadius);
	for (const AActor* SelectedStart : SelectedStarts)
	{
		if (Candidate == SelectedStart
			|| (SpawnClearanceRadius > 0.0f
				&& IsValid(SelectedStart)
				&& FVector::DistSquared(Candidate->GetActorLocation(), SelectedStart->GetActorLocation())
					< ClearanceRadiusSquared))
		{
			return false;
		}
	}

	return true;
}

bool UTeamPlayerManager::IsPlayerStartClear(const AActor* PlayerStart) const
{
	const ABaseTeamGameMode* GameMode = GetOwnerGameMode();
	UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
	if (!IsValid(PlayerStart) || !IsValid(World) || SpawnClearanceRadius <= 0.0f)
	{
		return true;
	}

	const float ClearanceRadiusSquared = FMath::Square(SpawnClearanceRadius);
	for (TActorIterator<APawn> Iterator(World); Iterator; ++Iterator)
	{
		const APawn* ExistingPawn = *Iterator;
		if (IsValid(ExistingPawn)
			&& FVector::DistSquared(ExistingPawn->GetActorLocation(), PlayerStart->GetActorLocation()) < ClearanceRadiusSquared)
		{
			return false;
		}
	}

	return true;
}

ABaseTeamGameMode* UTeamPlayerManager::GetOwnerGameMode() const
{
	return OwnerGameMode.Get();
}
