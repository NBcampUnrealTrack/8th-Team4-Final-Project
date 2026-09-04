#include "GameMode/State/LobbyGameState.h"

#include "Net/UnrealNetwork.h"

void ALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALobbyGameState, LobbyRevision);
	DOREPLIFETIME(ALobbyGameState, SelectedMatchMode);
}

void ALobbyGameState::NotifyLobbyPlayerListChanged()
{
	if (!HasAuthority())
	{
		return;
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

void ALobbyGameState::OnRep_LobbyRevision()
{
	OnLobbyPlayerListChanged.Broadcast();
}

void ALobbyGameState::OnRep_SelectedMatchMode()
{
	OnLobbyMatchModeChanged.Broadcast(SelectedMatchMode);
}
