#include "Player/BaseTeamPlayerState.h"

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
	DOREPLIFETIME(ABaseTeamPlayerState, bSpottedByEnemy);
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

bool ABaseTeamPlayerState::IsSpottedByEnemy() const
{
	return bSpottedByEnemy;
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

	// 죽는 순간 다음 판정 주기를 기다리지 않고 적 맵에서 바로 지운다.
	if (!bIsAlive)
	{
		SetSpottedByEnemy(false);
	}

	ForceNetUpdate();
}

void ABaseTeamPlayerState::SetSpottedByEnemy(const bool bNewIsSpotted)
{
	if (!HasAuthority() || bSpottedByEnemy == bNewIsSpotted)
	{
		return;
	}

	bSpottedByEnemy = bNewIsSpotted;
	OnRep_SpottedByEnemy();
	// PlayerState NetUpdateFrequency(10)를 기다리지 않고 바로 보낸다.
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

void ABaseTeamPlayerState::OnRep_SpottedByEnemy()
{
	OnSpottedByEnemyChanged.Broadcast(bSpottedByEnemy);
}
