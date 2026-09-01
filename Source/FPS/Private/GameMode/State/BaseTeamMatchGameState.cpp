#include "GameMode/State/BaseTeamMatchGameState.h"

#include "Net/UnrealNetwork.h"

void ABaseTeamMatchGameState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseTeamMatchGameState, MatchWinner);
}

EGameTeam ABaseTeamMatchGameState::GetMatchWinner() const
{
	return MatchWinner;
}

void ABaseTeamMatchGameState::SetMatchWinner(const EGameTeam WinningTeam)
{
	if (!HasAuthority())
	{
		return;
	}

	MatchWinner = WinningTeam;
	BroadcastMatchStateChanged();
	ForceNetUpdate();
}

void ABaseTeamMatchGameState::HandleMatchStateChanged()
{
}

void ABaseTeamMatchGameState::OnRep_MatchWinner()
{
	BroadcastMatchStateChanged();
}

void ABaseTeamMatchGameState::BroadcastMatchStateChanged()
{
	OnMatchStateChanged.Broadcast();
	HandleMatchStateChanged();
}
