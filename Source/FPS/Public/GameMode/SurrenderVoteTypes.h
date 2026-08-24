#pragma once

#include "CoreMinimal.h"
#include "GameMode/RoundTypes.h"
#include "GameMode/TeamTypes.h"
#include "SurrenderVoteTypes.generated.h"

UENUM(BlueprintType)
enum class ESurrenderVoteChoice : uint8
{
	Pending UMETA(DisplayName = "Pending"),
	Approve UMETA(DisplayName = "Approve"),
	Reject UMETA(DisplayName = "Reject")
};

UENUM(BlueprintType)
enum class ESurrenderVoteStartResult : uint8
{
	Started UMETA(DisplayName = "Started"),
	NotAllowed UMETA(DisplayName = "Not Allowed"),
	AlreadyActive UMETA(DisplayName = "Already Active"),
	Cooldown UMETA(DisplayName = "Cooldown")
};

USTRUCT(BlueprintType)
struct FSurrenderVoterState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Surrender")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Surrender")
	ESurrenderVoteChoice Choice = ESurrenderVoteChoice::Pending;
};

USTRUCT(BlueprintType)
struct FSurrenderVoteState
{
	GENERATED_BODY()

	bool IsActive() const
	{
		return bIsActive && VoteId > 0;
	}

	int32 GetApprovalCount() const
	{
		int32 ApprovalCount = 0;
		for (const FSurrenderVoterState& Voter : Voters)
		{
			ApprovalCount += Voter.Choice == ESurrenderVoteChoice::Approve ? 1 : 0;
		}
		return ApprovalCount;
	}

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Surrender")
	bool bIsActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Surrender")
	int32 VoteId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Surrender")
	EGameTeam SurrenderingTeam = EGameTeam::None;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Surrender")
	int32 RequiredApprovals = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Surrender")
	float EndServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Surrender")
	bool bLocalPlayerHasResponded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Surrender")
	TArray<FSurrenderVoterState> Voters;
};

namespace SurrenderVoteRules
{
	inline int32 CalculateRequiredApprovals(const int32 VoterCount)
	{
		return VoterCount / 2 + 1;
	}

	inline EGameTeam GetOpposingTeam(const EGameTeam Team)
	{
		switch (Team)
		{
		case EGameTeam::TeamA:
			return EGameTeam::TeamB;
		case EGameTeam::TeamB:
			return EGameTeam::TeamA;
		default:
			return EGameTeam::None;
		}
	}

	inline bool IsVotingPhase(const EGameFlowPhase Phase)
	{
		return Phase == EGameFlowPhase::RoundStarting
			|| Phase == EGameFlowPhase::AugmentSelecting
			|| Phase == EGameFlowPhase::RoundInProgress;
	}
}
