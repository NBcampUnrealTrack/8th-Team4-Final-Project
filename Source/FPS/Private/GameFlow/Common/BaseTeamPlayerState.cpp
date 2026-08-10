#include "GameFlow/Common/BaseTeamPlayerState.h"

#include "Net/UnrealNetwork.h"

ABaseTeamPlayerState::ABaseTeamPlayerState()
{
	SetNetUpdateFrequency(10.0f);
}

void ABaseTeamPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseTeamPlayerState, Team);
	DOREPLIFETIME(ABaseTeamPlayerState, bIsAlive);
}

void ABaseTeamPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	ABaseTeamPlayerState* NewTeamPlayerState = Cast<ABaseTeamPlayerState>(PlayerState);
	if (!IsValid(NewTeamPlayerState))
	{
		return;
	}

	NewTeamPlayerState->Team = Team;
	NewTeamPlayerState->bIsAlive = false;
}

void ABaseTeamPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);

	const ABaseTeamPlayerState* PreviousTeamPlayerState = Cast<ABaseTeamPlayerState>(PlayerState);
	if (!IsValid(PreviousTeamPlayerState))
	{
		return;
	}

	Team = PreviousTeamPlayerState->Team;
	bIsAlive = false;
}

EGameTeam ABaseTeamPlayerState::GetTeam() const
{
	return Team;
}

bool ABaseTeamPlayerState::IsAlive() const
{
	return bIsAlive;
}

void ABaseTeamPlayerState::SetTeam(const EGameTeam NewTeam)
{
	if (!HasAuthority() || Team == NewTeam)
	{
		return;
	}

	const EGameTeam PreviousTeam = Team;
	Team = NewTeam;
	OnRep_Team(PreviousTeam);
	ForceNetUpdate();
}

void ABaseTeamPlayerState::SetAlive(const bool bNewIsAlive)
{
	if (!HasAuthority() || bIsAlive == bNewIsAlive)
	{
		return;
	}

	bIsAlive = bNewIsAlive;
	OnRep_IsAlive();
	ForceNetUpdate();
}

void ABaseTeamPlayerState::OnRep_Team(const EGameTeam PreviousTeam)
{
	OnPlayerTeamChanged.Broadcast(PreviousTeam, Team);
}

void ABaseTeamPlayerState::OnRep_IsAlive()
{
	OnPlayerAliveStateChanged.Broadcast(bIsAlive);
}
