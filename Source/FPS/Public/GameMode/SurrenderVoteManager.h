#pragma once

#include "CoreMinimal.h"
#include "GameMode/SurrenderVoteTypes.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "SurrenderVoteManager.generated.h"

class ABaseTeamGameMode;
class AShooterPlayerController;

UCLASS()
class FPS_API USurrenderVoteManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ABaseTeamGameMode* InOwnerGameMode, float InVoteDuration, float InVoteCooldown);
	void Shutdown();

	ESurrenderVoteStartResult TryStartVote(
		EGameTeam Team,
		const TArray<AShooterPlayerController*>& Voters);

	EGameTeam SubmitVote(
		EGameTeam Team,
		AShooterPlayerController* Voter,
		int32 VoteId,
		bool bApprove);

	void HandleVoterLeaving(AShooterPlayerController* Voter);
	void ResetForNewMatch();

private:
	struct FTeamVoteRuntime
	{
		FSurrenderVoteState State;
		TArray<TWeakObjectPtr<AShooterPlayerController>> Voters;
		FTimerHandle ExpirationTimerHandle;
	};

	void PublishVoteState(const FTeamVoteRuntime& Vote) const;
	void CancelAllActiveVotes();
	void CloseVote(EGameTeam Team, bool bStartCooldown);
	void HandleVoteExpired(EGameTeam Team, int32 VoteId);
	float GetServerTime() const;

	TWeakObjectPtr<ABaseTeamGameMode> OwnerGameMode;
	TMap<EGameTeam, FTeamVoteRuntime> ActiveVotes;
	TMap<EGameTeam, float> NextVoteAllowedTimes;

	float VoteDuration = 0.0f;
	float VoteCooldown = 0.0f;
	int32 NextVoteId = 1;
};
