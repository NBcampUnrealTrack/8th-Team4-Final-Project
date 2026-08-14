#include "GameFlow/Lobby/LobbyGameMode.h"

#include "GameFlow/Lobby/LobbyGameState.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"

ALobbyGameMode::ALobbyGameMode()
{
	GameStateClass = ALobbyGameState::StaticClass();
	PlayerControllerClass = AShooterPlayerController::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	NotifyLobbyPlayerListChanged();
}

void ALobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	NotifyLobbyPlayerListChanged();
}

void ALobbyGameMode::NotifyLobbyPlayerListChanged()
{
	if (ALobbyGameState* LobbyGameState = GetGameState<ALobbyGameState>())
	{
		LobbyGameState->NotifyLobbyPlayerListChanged();
	}
}
