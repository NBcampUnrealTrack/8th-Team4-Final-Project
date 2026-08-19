#include "GameMode/LobbyGameState.h"

#include "Net/UnrealNetwork.h"

void ALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALobbyGameState, LobbyRevision);
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

void ALobbyGameState::OnRep_LobbyRevision()
{
	OnLobbyPlayerListChanged.Broadcast();
}
