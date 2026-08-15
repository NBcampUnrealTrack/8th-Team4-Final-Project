#include "GameFlow/Common/TeamPlayerManager.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFlow/Common/BaseTeamGameMode.h"
#include "GameFlow/Common/BaseTeamGameState.h"
#include "GameFlow/Common/BaseTeamPlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogTeamPlayerManager, Log, All);

void UTeamPlayerManager::Initialize(ABaseTeamGameMode* InOwnerGameMode, const float InSpawnClearanceRadius)
{
	OwnerGameMode = InOwnerGameMode;
	SpawnClearanceRadius = FMath::Max(0.0f, InSpawnClearanceRadius);
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

AActor* UTeamPlayerManager::SelectAvailablePlayerStart(
	const TArray<AActor*>& MatchingStarts,
	const TCHAR* StartDescription) const
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

		ValidStarts.Add(PlayerStart);
		if (IsPlayerStartClear(PlayerStart))
		{
			ClearStarts.Add(PlayerStart);
		}
	}

	if (ValidStarts.IsEmpty())
	{
		UE_LOG(LogTeamPlayerManager, Error, TEXT("No valid %s is configured for the requested spawn group."), StartDescription);
		return nullptr;
	}

	if (!ClearStarts.IsEmpty())
	{
		return ClearStarts[FMath::RandHelper(ClearStarts.Num())];
	}

	if (SpawnClearanceRadius <= 0.0f)
	{
		return ValidStarts[FMath::RandHelper(ValidStarts.Num())];
	}

	UE_LOG(LogTeamPlayerManager, Error,
		TEXT("No clear %s is available. Add enough starts or reduce SpawnClearanceRadius."),
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

bool UTeamPlayerManager::PreparePlayersForRound()
{
	ABaseTeamGameMode* GameMode = GetOwnerGameMode();
	if (!IsValid(GameMode))
	{
		return false;
	}

	TArray<APlayerController*> Players;
	if (!GatherEligiblePlayers(Players))
	{
		return false;
	}

	ResetRoundTracking();
	for (APlayerController* PlayerController : Players)
	{
		RemovePlayerPawn(PlayerController);
	}

	for (APlayerController* PlayerController : Players)
	{
		AShooterPlayerState* PlayerState = PlayerController->GetPlayerState<AShooterPlayerState>();
		GameMode->RestartPlayer(PlayerController);

		APawn* SpawnedPawn = PlayerController->GetPawn();
		if (!IsValid(PlayerState)
			|| !IsValid(SpawnedPawn)
			|| SpawnedPawn->GetController() != PlayerController
			|| PlayerController->GetPlayerState<AShooterPlayerState>() != PlayerState)
		{
			UE_LOG(LogTeamPlayerManager, Error,
				TEXT("Failed to spawn %s for the round. Check DefaultPawnClass and the mode-specific PlayerStarts."),
				*GetNameSafe(PlayerController));
			CleanupFailedPreparation(Players);
			return false;
		}

		PlayerState->ServerRespawnReset();
		if (!PlayerState->IsAlive())
		{
			UE_LOG(LogTeamPlayerManager, Error,
				TEXT("%s remained dead after ServerRespawnReset()."),
				*GetNameSafe(PlayerState));
			CleanupFailedPreparation(Players);
			return false;
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
		return false;
	}

	return true;
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
	ResetRoundTracking();
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
