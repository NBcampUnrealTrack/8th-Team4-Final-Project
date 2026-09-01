#include "GameMode/State/BaseTeamGameState.h"

#include "Net/UnrealNetwork.h"

ABaseTeamGameState::ABaseTeamGameState()
{
	SetNetUpdateFrequency(10.0f);
}

void ABaseTeamGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseTeamGameState, TeamAScore);
	DOREPLIFETIME(ABaseTeamGameState, TeamBScore);
}

int32 ABaseTeamGameState::GetTeamScore(const EGameTeam Team) const
{
	switch (Team)
	{
	case EGameTeam::TeamA:
		return TeamAScore;
	case EGameTeam::TeamB:
		return TeamBScore;
	default:
		return 0;
	}
}

int32 ABaseTeamGameState::GetTeamAScore() const
{
	return TeamAScore;
}

int32 ABaseTeamGameState::GetTeamBScore() const
{
	return TeamBScore;
}

EGameTeam ABaseTeamGameState::GetLeadingTeam() const
{
	if (TeamAScore == TeamBScore)
	{
		return EGameTeam::None;
	}

	return TeamAScore > TeamBScore ? EGameTeam::TeamA : EGameTeam::TeamB;
}

void ABaseTeamGameState::AddTeamScore(const EGameTeam Team, const int32 ScoreAmount)
{
	if (!HasAuthority() || ScoreAmount <= 0)
	{
		return;
	}

	switch (Team)
	{
	case EGameTeam::TeamA:
		TeamAScore += ScoreAmount;
		OnRep_TeamAScore();
		break;
	case EGameTeam::TeamB:
		TeamBScore += ScoreAmount;
		OnRep_TeamBScore();
		break;
	default:
		return;
	}

	ForceNetUpdate();
}

void ABaseTeamGameState::ResetTeamScores()
{
	if (!HasAuthority())
	{
		return;
	}

	TeamAScore = 0;
	TeamBScore = 0;
	OnRep_TeamAScore();
	OnRep_TeamBScore();
	ForceNetUpdate();
}

void ABaseTeamGameState::BroadcastPlayerEliminated(const FShooterEliminationEvent& EliminationEvent)
{
	if (!HasAuthority())
	{
		return;
	}

	Multicast_PlayerEliminated(EliminationEvent);
}

void ABaseTeamGameState::Multicast_PlayerEliminated_Implementation(const FShooterEliminationEvent& EliminationEvent)
{
	OnPlayerEliminated.Broadcast(EliminationEvent);
}

void ABaseTeamGameState::OnRep_TeamAScore()
{
	OnTeamScoreChanged.Broadcast(EGameTeam::TeamA, TeamAScore);
}

void ABaseTeamGameState::OnRep_TeamBScore()
{
	OnTeamScoreChanged.Broadcast(EGameTeam::TeamB, TeamBScore);
}
