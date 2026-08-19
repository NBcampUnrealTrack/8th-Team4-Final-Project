#include "GameMode/InitialPlayerReadinessTracker.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/BaseTeamPlayerState.h"

bool FInitialPlayerReadinessSnapshot::AreAllAssignedPlayersReady(const int32 MinimumPlayerCount) const
{
	return AssignedPlayerCount >= MinimumPlayerCount
		&& ReadyPlayerCount == AssignedPlayerCount;
}

void FInitialPlayerReadinessTracker::ResetTimeoutForSameMapRestart()
{
	bTimeoutExpired = false;
}

bool FInitialPlayerReadinessTracker::MarkPlayerReady(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return false;
	}

	const int32 PreviousReadyPlayerCount = ReadyPlayerControllers.Num();
	ReadyPlayerControllers.Add(TWeakObjectPtr<APlayerController>(PlayerController));
	return ReadyPlayerControllers.Num() != PreviousReadyPlayerCount;
}

void FInitialPlayerReadinessTracker::RemovePlayer(APlayerController* PlayerController)
{
	ReadyPlayerControllers.Remove(TWeakObjectPtr<APlayerController>(PlayerController));
}

bool FInitialPlayerReadinessTracker::IsPlayerReady(APlayerController* PlayerController) const
{
	return IsValid(PlayerController)
		&& ReadyPlayerControllers.Contains(TWeakObjectPtr<APlayerController>(PlayerController));
}

void FInitialPlayerReadinessTracker::MarkTimeoutExpired()
{
	bTimeoutExpired = true;
}

bool FInitialPlayerReadinessTracker::HasTimeoutExpired() const
{
	return bTimeoutExpired;
}

FInitialPlayerReadinessSnapshot FInitialPlayerReadinessTracker::CaptureSnapshot(const UWorld* World) const
{
	FInitialPlayerReadinessSnapshot Snapshot;
	if (!IsValid(World))
	{
		return Snapshot;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		const ABaseTeamPlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ABaseTeamPlayerState>()
			: nullptr;
		if (!IsValid(PlayerState) || PlayerState->GetTeam() == EGameTeam::None)
		{
			continue;
		}

		++Snapshot.AssignedPlayerCount;
		Snapshot.AssignedPlayerControllers.Add(PlayerController);

		if (IsPlayerReady(PlayerController))
		{
			++Snapshot.ReadyPlayerCount;
			continue;
		}

		const FString PlayerName = PlayerState->GetPlayerName();
		Snapshot.UnreadyPlayerNames.Add(
			PlayerName.IsEmpty() ? GetNameSafe(PlayerController) : PlayerName);
	}

	return Snapshot;
}
