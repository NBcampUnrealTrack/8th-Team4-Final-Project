#pragma once

#include "CoreMinimal.h"
#include "GameMode/SurrenderVoteTypes.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "SurrenderVoteManager.generated.h"

class ABaseRoundGameMode;
class ABaseRoundPlayerController;

UCLASS()
class FPS_API USurrenderVoteManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ABaseRoundGameMode* InOwnerGameMode, float InVoteDuration, float InVoteCooldown);
	void Shutdown();

	ESurrenderVoteStartResult TryStartVote(
		EGameTeam Team,
		const TArray<ABaseRoundPlayerController*>& Voters);

	EGameTeam SubmitVote(
		EGameTeam Team,
		ABaseRoundPlayerController* Voter,
		int32 VoteId,
		bool bApprove);

	void HandleVoterLeaving(ABaseRoundPlayerController* Voter);
	void CancelAllActiveVotes();
	void ResetForNewMatch();

private:
	struct FTeamVoteRuntime
	{
		FSurrenderVoteState State;
		TArray<TWeakObjectPtr<ABaseRoundPlayerController>> Voters;
		FTimerHandle ExpirationTimerHandle;
	};

	void PublishVoteState(const FTeamVoteRuntime& Vote) const;
	void CloseVote(EGameTeam Team, bool bStartCooldown);
	void HandleVoteExpired(EGameTeam Team, int32 VoteId);
	float GetServerTime() const;

	TWeakObjectPtr<ABaseRoundGameMode> OwnerGameMode;
	TMap<EGameTeam, FTeamVoteRuntime> ActiveVotes;
	TMap<EGameTeam, float> NextVoteAllowedTimes;

	float VoteDuration = 0.0f;
	float VoteCooldown = 0.0f;
	int32 NextVoteId = 1;
};
