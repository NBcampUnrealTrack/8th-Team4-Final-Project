#include "GameFlow/Common/BaseTeamGameMode.h"

#include "EngineUtils.h"
#include "GameFlow/Common/BaseTeamGameState.h"
#include "GameFlow/Common/BaseTeamPlayerStart.h"
#include "GameFlow/Common/BaseTeamPlayerState.h"
#include "GameFlow/Common/TeamPlayerManager.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogBaseTeamGameMode, Log, All);

ABaseTeamGameMode::ABaseTeamGameMode()
{
	GameStateClass = ABaseTeamGameState::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	DefaultPawnClass = nullptr;

	TeamPlayerManager = CreateDefaultSubobject<UTeamPlayerManager>(TEXT("TeamPlayerManager"));
}

int32 ABaseTeamGameMode::GetTeamPlayerCount(const EGameTeam Team) const
{
	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	return IsValid(PlayerManager) ? PlayerManager->GetTeamPlayerCount(Team) : 0;
}

void ABaseTeamGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (IsValid(TeamPlayerManager))
	{
		TeamPlayerManager->Initialize(this, SpawnClearanceRadius);
	}
}

void ABaseTeamGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!InitializeTeamPlayer(NewPlayer))
	{
		return;
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

AActor* ABaseTeamGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const ABaseTeamPlayerState* TeamPlayerState = IsValid(Player)
		? Player->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	if (!IsValid(TeamPlayerState) || TeamPlayerState->GetTeam() == EGameTeam::None)
	{
		UE_LOG(LogBaseTeamGameMode, Error, TEXT("Cannot choose a team spawn because %s has no assigned team."), *GetNameSafe(Player));
		return nullptr;
	}

	TArray<AActor*> MatchingStarts;
	for (TActorIterator<ABaseTeamPlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ABaseTeamPlayerStart* PlayerStart = *Iterator;
		if (IsValid(PlayerStart) && PlayerStart->GetSpawnTeam() == TeamPlayerState->GetTeam())
		{
			MatchingStarts.Add(PlayerStart);
		}
	}

	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	return IsValid(PlayerManager)
		? PlayerManager->SelectAvailablePlayerStart(MatchingStarts, TEXT("BaseTeamPlayerStart"))
		: nullptr;
}

EGameTeam ABaseTeamGameMode::ChooseTeamForNewPlayer() const
{
	const int32 TeamACount = GetTeamPlayerCount(EGameTeam::TeamA);
	const int32 TeamBCount = GetTeamPlayerCount(EGameTeam::TeamB);
	return TeamACount <= TeamBCount ? EGameTeam::TeamA : EGameTeam::TeamB;
}

bool ABaseTeamGameMode::InitializeTeamPlayer(APlayerController* PlayerController)
{
	UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	return IsValid(PlayerManager)
		&& PlayerManager->AssignTeamIfNeeded(PlayerController, ChooseTeamForNewPlayer());
}

ABaseTeamGameState* ABaseTeamGameMode::GetBaseTeamGameState() const
{
	return GetGameState<ABaseTeamGameState>();
}

int32 ABaseTeamGameMode::GetAlivePlayerCount(const EGameTeam Team) const
{
	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	return IsValid(PlayerManager) ? PlayerManager->GetAlivePlayerCount(Team) : 0;
}

UTeamPlayerManager* ABaseTeamGameMode::GetTeamPlayerManager() const
{
	return TeamPlayerManager.Get();
}
