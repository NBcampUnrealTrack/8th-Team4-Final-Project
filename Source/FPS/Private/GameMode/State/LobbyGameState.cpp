#include "GameMode/State/LobbyGameState.h"

#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/LobbyGameMode.h"
#include "Net/UnrealNetwork.h"

void ALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALobbyGameState, LobbyRevision);
	DOREPLIFETIME(ALobbyGameState, SelectedMatchMode);
	DOREPLIFETIME(ALobbyGameState, RoomSettings);
	DOREPLIFETIME(ALobbyGameState, HostPlayerState);
	DOREPLIFETIME(ALobbyGameState, StartState);
}

void ALobbyGameState::NotifyLobbyPlayerListChanged()
{
	if (!HasAuthority())
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		if (const ALobbyGameMode* LobbyGameMode = World->GetAuthGameMode<ALobbyGameMode>())
		{
			StartState = LobbyGameMode->BuildStartState();
		}
	}

	++LobbyRevision;
	OnLobbyPlayerListChanged.Broadcast();
	ForceNetUpdate();
}

void ALobbyGameState::SetSelectedMatchMode(const ELobbyMatchMode NewMatchMode)
{
	if (!HasAuthority() || SelectedMatchMode == NewMatchMode)
	{
		return;
	}

	SelectedMatchMode = NewMatchMode;
	OnRep_SelectedMatchMode();
	ForceNetUpdate();
}

void ALobbyGameState::SetRoomSettings(const FLobbyRoomSettings& NewRoomSettings)
{
	if (!HasAuthority())
	{
		return;
	}

	RoomSettings = NewRoomSettings;
}

void ALobbyGameState::SetHostPlayerState(APlayerState* NewHostPlayerState)
{
	if (!HasAuthority())
	{
		return;
	}

	HostPlayerState = NewHostPlayerState;
}

void ALobbyGameState::OnRep_LobbyRevision()
{
	OnLobbyPlayerListChanged.Broadcast();
}

void ALobbyGameState::OnRep_SelectedMatchMode()
{
	OnLobbyMatchModeChanged.Broadcast(SelectedMatchMode);
}

void ALobbyGameState::OnRep_RoomSettings()
{
	OnLobbyPlayerListChanged.Broadcast();
}

void ALobbyGameState::OnRep_HostPlayerState()
{
	OnLobbyPlayerListChanged.Broadcast();
}

void ALobbyGameState::OnRep_StartState()
{
	OnLobbyPlayerListChanged.Broadcast();
}
