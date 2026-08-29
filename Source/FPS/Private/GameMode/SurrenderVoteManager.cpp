#include "GameMode/SurrenderVoteManager.h"

#include "Engine/World.h"
#include "GameMode/BaseTeamGameMode.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerController.h"

void USurrenderVoteManager::Initialize(
	ABaseTeamGameMode* InOwnerGameMode,
	const float InVoteDuration,
	const float InVoteCooldown)
{
	Shutdown();

	OwnerGameMode = InOwnerGameMode;
	VoteDuration = InVoteDuration;
	VoteCooldown = InVoteCooldown;
}

void USurrenderVoteManager::Shutdown()
{
	ResetForNewMatch();
	OwnerGameMode.Reset();
	VoteDuration = 0.0f;
	VoteCooldown = 0.0f;
}

ESurrenderVoteStartResult USurrenderVoteManager::TryStartVote(
	const EGameTeam Team,
	const TArray<AShooterPlayerController*>& Voters)
{
	if (ActiveVotes.Contains(Team))
	{
		return ESurrenderVoteStartResult::AlreadyActive;
	}

	const float ServerTime = GetServerTime();
	if (ServerTime < NextVoteAllowedTimes.FindRef(Team))
	{
		return ESurrenderVoteStartResult::Cooldown;
	}

	FTeamVoteRuntime Vote;
	Vote.State.bIsActive = true;
	Vote.State.VoteId = NextVoteId++;
	Vote.State.SurrenderingTeam = Team;
	Vote.State.RequiredApprovals = SurrenderVoteRules::CalculateRequiredApprovals(Voters.Num());
	Vote.State.EndServerTime = ServerTime + VoteDuration;
	Vote.State.Voters.Reserve(Voters.Num());
	Vote.Voters.Reserve(Voters.Num());

	for (AShooterPlayerController* Voter : Voters)
	{
		const APlayerState* PlayerState = Voter->GetPlayerState<APlayerState>();

		FSurrenderVoterState VoterState;
		VoterState.PlayerName = PlayerState->GetPlayerName();
		Vote.State.Voters.Add(MoveTemp(VoterState));
		Vote.Voters.Add(Voter);
	}

	const int32 VoteId = Vote.State.VoteId;
	FTeamVoteRuntime& ActiveVote = ActiveVotes.Add(Team, MoveTemp(Vote));

	FTimerDelegate ExpirationDelegate = FTimerDelegate::CreateUObject(
		this,
		&ThisClass::HandleVoteExpired,
		Team,
		VoteId);
	OwnerGameMode->GetWorldTimerManager().SetTimer(
		ActiveVote.ExpirationTimerHandle,
		ExpirationDelegate,
		VoteDuration,
		false);

	PublishVoteState(ActiveVote);
	return ESurrenderVoteStartResult::Started;
}

EGameTeam USurrenderVoteManager::SubmitVote(
	const EGameTeam Team,
	AShooterPlayerController* Voter,
	const int32 VoteId,
	const bool bApprove)
{
	FTeamVoteRuntime* Vote = ActiveVotes.Find(Team);
	if (Vote == nullptr || Vote->State.VoteId != VoteId)
	{
		return EGameTeam::None;
	}

	const int32 VoterIndex = Vote->Voters.IndexOfByPredicate(
		[Voter](const TWeakObjectPtr<AShooterPlayerController>& Candidate)
		{
			return Candidate.Get() == Voter;
		});

	if (VoterIndex == INDEX_NONE
		|| Vote->State.Voters[VoterIndex].Choice != ESurrenderVoteChoice::Pending)
	{
		return EGameTeam::None;
	}

	Vote->State.Voters[VoterIndex].Choice = bApprove
		? ESurrenderVoteChoice::Approve
		: ESurrenderVoteChoice::Reject;

	int32 ApprovalCount = 0;
	int32 PendingCount = 0;
	for (const FSurrenderVoterState& VoterState : Vote->State.Voters)
	{
		ApprovalCount += VoterState.Choice == ESurrenderVoteChoice::Approve ? 1 : 0;
		PendingCount += VoterState.Choice == ESurrenderVoteChoice::Pending ? 1 : 0;
	}

	if (ApprovalCount >= Vote->State.RequiredApprovals)
	{
		CancelAllActiveVotes();
		return Team;
	}

	if (ApprovalCount + PendingCount < Vote->State.RequiredApprovals)
	{
		CloseVote(Team, true);
		return EGameTeam::None;
	}

	PublishVoteState(*Vote);
	return EGameTeam::None;
}

void USurrenderVoteManager::HandleVoterLeaving(AShooterPlayerController* Voter)
{
	const EGameTeam Teams[] = {EGameTeam::TeamA, EGameTeam::TeamB};
	for (const EGameTeam Team : Teams)
	{
		const FTeamVoteRuntime* Vote = ActiveVotes.Find(Team);
		const bool bBelongsToVote = Vote != nullptr
			&& Vote->Voters.ContainsByPredicate(
				[Voter](const TWeakObjectPtr<AShooterPlayerController>& Candidate)
				{
					return Candidate.Get() == Voter;
				});

		if (bBelongsToVote)
		{
			CloseVote(Team, true);
		}
	}
}

void USurrenderVoteManager::CancelAllActiveVotes()
{
	CloseVote(EGameTeam::TeamA, true);
	CloseVote(EGameTeam::TeamB, true);
}

void USurrenderVoteManager::ResetForNewMatch()
{
	CloseVote(EGameTeam::TeamA, false);
	CloseVote(EGameTeam::TeamB, false);
	NextVoteAllowedTimes.Reset();
	NextVoteId = 1;
}

void USurrenderVoteManager::PublishVoteState(const FTeamVoteRuntime& Vote) const
{
	for (int32 VoterIndex = 0; VoterIndex < Vote.Voters.Num(); ++VoterIndex)
	{
		AShooterPlayerController* Voter = Vote.Voters[VoterIndex].Get();
		if (!IsValid(Voter))
		{
			continue;
		}

		FSurrenderVoteState OwnerState = Vote.State;
		OwnerState.bLocalPlayerHasResponded =
			Vote.State.Voters[VoterIndex].Choice != ESurrenderVoteChoice::Pending;
		Voter->SetSurrenderVoteState(OwnerState);
	}
}

void USurrenderVoteManager::CloseVote(const EGameTeam Team, const bool bStartCooldown)
{
	FTeamVoteRuntime* Vote = ActiveVotes.Find(Team);
	if (Vote == nullptr)
	{
		return;
	}

	OwnerGameMode->GetWorldTimerManager().ClearTimer(Vote->ExpirationTimerHandle);
	if (bStartCooldown)
	{
		NextVoteAllowedTimes.Add(Team, GetServerTime() + VoteCooldown);
	}

	for (const TWeakObjectPtr<AShooterPlayerController>& VoterPtr : Vote->Voters)
	{
		if (AShooterPlayerController* Voter = VoterPtr.Get())
		{
			Voter->SetSurrenderVoteState(FSurrenderVoteState());
		}
	}

	ActiveVotes.Remove(Team);
}

void USurrenderVoteManager::HandleVoteExpired(const EGameTeam Team, const int32 VoteId)
{
	const FTeamVoteRuntime* Vote = ActiveVotes.Find(Team);
	if (Vote != nullptr && Vote->State.VoteId == VoteId)
	{
		CloseVote(Team, true);
	}
}

float USurrenderVoteManager::GetServerTime() const
{
	return OwnerGameMode->GetWorld()->GetTimeSeconds();
}
