#include "GameFlow/Common/BaseTeamGameMode.h"

#include "EngineUtils.h"
#include "GameFlow/Common/BaseTeamGameState.h"
#include "GameFlow/Common/BaseTeamPlayerStart.h"
#include "GameFlow/Common/BaseTeamPlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogBaseTeamGameMode, Log, All);

ABaseTeamGameMode::ABaseTeamGameMode()
{
	GameStateClass = ABaseTeamGameState::StaticClass();
	PlayerStateClass = ABaseTeamPlayerState::StaticClass();
	DefaultPawnClass = nullptr;
}

int32 ABaseTeamGameMode::GetTeamPlayerCount(const EGameTeam Team) const
{
	const ABaseTeamGameState* TeamGameState = GetBaseTeamGameState();
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

void ABaseTeamGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	EnsurePlayerTeamAssigned(NewPlayer);
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

AActor* ABaseTeamGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const ABaseTeamPlayerState* TeamPlayerState = IsValid(Player) ? Player->GetPlayerState<ABaseTeamPlayerState>() : nullptr;
	if (!IsValid(TeamPlayerState) || TeamPlayerState->GetTeam() == EGameTeam::None)
	{
		UE_LOG(LogBaseTeamGameMode, Error, TEXT("Cannot choose a team spawn because %s has no assigned team."), *GetNameSafe(Player));
		return nullptr;
	}

	TArray<ABaseTeamPlayerStart*> MatchingStarts;
	for (TActorIterator<ABaseTeamPlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ABaseTeamPlayerStart* PlayerStart = *Iterator;
		if (IsValid(PlayerStart) && PlayerStart->GetSpawnTeam() == TeamPlayerState->GetTeam())
		{
			MatchingStarts.Add(PlayerStart);
		}
	}

	if (MatchingStarts.Num() == 0)
	{
		UE_LOG(LogBaseTeamGameMode, Error, TEXT("No BaseTeamPlayerStart is configured for team %d."), static_cast<int32>(TeamPlayerState->GetTeam()));
		return nullptr;
	}

	TArray<ABaseTeamPlayerStart*> ClearStarts;
	for (ABaseTeamPlayerStart* PlayerStart : MatchingStarts)
	{
		if (IsPlayerStartClear(PlayerStart))
		{
			ClearStarts.Add(PlayerStart);
		}
	}

	const TArray<ABaseTeamPlayerStart*>& CandidateStarts = ClearStarts.Num() == 0 ? MatchingStarts : ClearStarts;
	return CandidateStarts[FMath::RandHelper(CandidateStarts.Num())];
}

EGameTeam ABaseTeamGameMode::ChooseTeamForNewPlayer() const
{
	const int32 TeamACount = GetTeamPlayerCount(EGameTeam::TeamA);
	const int32 TeamBCount = GetTeamPlayerCount(EGameTeam::TeamB);
	return TeamACount <= TeamBCount ? EGameTeam::TeamA : EGameTeam::TeamB;
}

void ABaseTeamGameMode::EnsurePlayerTeamAssigned(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	ABaseTeamPlayerState* TeamPlayerState = PlayerController->GetPlayerState<ABaseTeamPlayerState>();
	if (!IsValid(TeamPlayerState))
	{
		UE_LOG(LogBaseTeamGameMode, Error, TEXT("%s does not use ABaseTeamPlayerState."), *GetNameSafe(PlayerController));
		return;
	}

	if (TeamPlayerState->GetTeam() != EGameTeam::None)
	{
		return;
	}

	TeamPlayerState->SetTeam(ChooseTeamForNewPlayer());
	TeamPlayerState->SetAlive(false);
}

ABaseTeamGameState* ABaseTeamGameMode::GetBaseTeamGameState() const
{
	return GetGameState<ABaseTeamGameState>();
}

int32 ABaseTeamGameMode::GetAlivePlayerCount(const EGameTeam Team) const
{
	const ABaseTeamGameState* TeamGameState = GetBaseTeamGameState();
	if (!IsValid(TeamGameState))
	{
		return 0;
	}

	int32 AlivePlayerCount = 0;
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

bool ABaseTeamGameMode::IsPlayerStartClear(const AActor* PlayerStart) const
{
	if (!IsValid(PlayerStart) || SpawnClearanceRadius <= 0.0f)
	{
		return true;
	}

	const float ClearanceRadiusSquared = FMath::Square(SpawnClearanceRadius);
	for (TActorIterator<APawn> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		const APawn* ExistingPawn = *Iterator;
		if (IsValid(ExistingPawn) && FVector::DistSquared(ExistingPawn->GetActorLocation(), PlayerStart->GetActorLocation()) < ClearanceRadiusSquared)
		{
			return false;
		}
	}

	return true;
}
