#include "GameMode/BaseRoundGameMode.h"

#include "Engine/World.h"
#include "GameMode/BaseRoundGameState.h"
#include "GameMode/SurrenderVoteManager.h"
#include "Player/BaseRoundPlayerController.h"
#include "Player/BaseTeamPlayerState.h"

ESurrenderVoteStartResult ABaseRoundGameMode::RequestStartSurrenderVote(
	ABaseRoundPlayerController* Requester)
{
	EGameTeam Team = EGameTeam::None;
	TArray<ABaseRoundPlayerController*> Voters;
	if (!BuildSurrenderVoteRequest(Requester, Team, Voters))
	{
		return ESurrenderVoteStartResult::NotAllowed;
	}

	return GetSurrenderVoteManager()->TryStartVote(Team, Voters);
}

void ABaseRoundGameMode::SubmitSurrenderVote(
	ABaseRoundPlayerController* Voter,
	const int32 VoteId,
	const bool bApprove)
{
	if (!HasAuthority() || !IsValid(Voter) || Voter->GetWorld() != GetWorld())
	{
		return;
	}

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	const ABaseTeamPlayerState* PlayerState = Voter->GetPlayerState<ABaseTeamPlayerState>();
	if (!IsValid(RoundGameState)
		|| !SurrenderVoteRules::IsVotingPhase(RoundGameState->GetGameFlowPhase())
		|| !IsValid(PlayerState)
		|| PlayerState->GetTeam() == EGameTeam::None)
	{
		return;
	}

	const EGameTeam SurrenderingTeam = GetSurrenderVoteManager()->SubmitVote(
		PlayerState->GetTeam(),
		Voter,
		VoteId,
		bApprove);

	if (SurrenderingTeam != EGameTeam::None)
	{
		FinishMatchBySurrender(SurrenderingTeam);
	}
}

bool ABaseRoundGameMode::BuildSurrenderVoteRequest(
	ABaseRoundPlayerController* Requester,
	EGameTeam& OutTeam,
	TArray<ABaseRoundPlayerController*>& OutVoters) const
{
	OutTeam = EGameTeam::None;
	OutVoters.Reset();

	if (!HasAuthority()
		|| !IsValid(Requester)
		|| Requester->GetWorld() != GetWorld()
		|| !HasMatchStarted()
		|| HasMatchEnded())
	{
		return false;
	}

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	const ABaseTeamPlayerState* RequesterState = Requester->GetPlayerState<ABaseTeamPlayerState>();
	if (!IsValid(RoundGameState)
		|| !SurrenderVoteRules::IsVotingPhase(RoundGameState->GetGameFlowPhase())
		|| !IsValid(RequesterState)
		|| RequesterState->GetTeam() == EGameTeam::None
		|| GetTeamPlayerCount(EGameTeam::TeamA) <= 0
		|| GetTeamPlayerCount(EGameTeam::TeamB) <= 0)
	{
		return false;
	}

	OutTeam = RequesterState->GetTeam();
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		ABaseRoundPlayerController* Candidate = Cast<ABaseRoundPlayerController>(Iterator->Get());
		const ABaseTeamPlayerState* CandidateState = IsValid(Candidate)
			? Candidate->GetPlayerState<ABaseTeamPlayerState>()
			: nullptr;
		if (IsValid(CandidateState) && CandidateState->GetTeam() == OutTeam)
		{
			OutVoters.Add(Candidate);
		}
	}

	return OutVoters.Contains(Requester);
}

void ABaseRoundGameMode::FinishMatchBySurrender(const EGameTeam SurrenderingTeam)
{
	const EGameTeam WinningTeam = SurrenderVoteRules::GetOpposingTeam(SurrenderingTeam);
	if (WinningTeam == EGameTeam::None)
	{
		return;
	}

	ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	if (RoundGameState->GetGameFlowPhase() == EGameFlowPhase::RoundInProgress)
	{
		DeactivateModeRound(WinningTeam, ERoundEndReason::Surrender);
	}
	else
	{
		ResetModeState();
	}

	RoundGameState->SetRoundResult(WinningTeam, ERoundEndReason::Surrender);
	FinishMatch(WinningTeam);
}
