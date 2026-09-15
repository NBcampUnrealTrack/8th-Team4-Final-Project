#include "GameMode/State/BaseTeamGameState.h"

#include "Net/UnrealNetwork.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerController.h"

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

void ABaseTeamGameState::ForEachChatRecipient(
	const APlayerState* Sender, const EChatChannel Channel, TFunctionRef<void(AShooterPlayerController*)> Visitor) const
{
	if (!HasAuthority() || !IsValid(Sender))
	{
		return;
	}

	const EGameTeam SenderTeam = Channel == EChatChannel::Team
		? (IsValid(Cast<ABaseTeamPlayerState>(Sender)) ? Cast<ABaseTeamPlayerState>(Sender)->GetTeam() : EGameTeam::None)
		: EGameTeam::None;

	for (APlayerState* PlayerState : PlayerArray)
	{
		if (Channel == EChatChannel::Team)
		{
			const ABaseTeamPlayerState* RecipientTeamState = Cast<ABaseTeamPlayerState>(PlayerState);
			if (!IsValid(RecipientTeamState) || RecipientTeamState->GetTeam() != SenderTeam)
			{
				continue;
			}
		}

		if (AShooterPlayerController* RecipientController = Cast<AShooterPlayerController>(PlayerState->GetPlayerController()))
		{
			Visitor(RecipientController);
		}
	}
}

AShooterPlayerController* ABaseTeamGameState::FindChatRecipientByName(const FString& PlayerName) const
{
	if (!HasAuthority())
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : PlayerArray)
	{
		if (!IsValid(PlayerState) || !PlayerState->GetPlayerName().Equals(PlayerName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		return Cast<AShooterPlayerController>(PlayerState->GetPlayerController());
	}

	return nullptr;
}

TArray<FString> ABaseTeamGameState::GetPlayerNamesMatchingPrefix(
	const FString& Prefix, const FString& ExcludeName) const
{
	TArray<FString> Results;
	if (Prefix.IsEmpty())
	{
		return Results;
	}

	for (const APlayerState* PlayerState : PlayerArray)
	{
		if (!IsValid(PlayerState))
		{
			continue;
		}

		const FString Name = PlayerState->GetPlayerName();
		if (!Name.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!ExcludeName.IsEmpty() && Name.Equals(ExcludeName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		Results.Add(Name);
	}

	return Results;
}

void ABaseTeamGameState::OnRep_TeamAScore()
{
	OnTeamScoreChanged.Broadcast(EGameTeam::TeamA, TeamAScore);
}

void ABaseTeamGameState::OnRep_TeamBScore()
{
	OnTeamScoreChanged.Broadcast(EGameTeam::TeamB, TeamBScore);
}
