#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Common/TeamTypes.h"
#include "RoundTypes.generated.h"

// Round-only roles and states are separated so team deathmatch and domination can reuse the common layer.
UENUM(BlueprintType)
enum class ETeamRole : uint8
{
	None UMETA(DisplayName = "None"),
	Attackers UMETA(DisplayName = "Attackers"),
	Defenders UMETA(DisplayName = "Defenders")
};

// Phase values drive replicated round transitions and WBP visibility rules.
UENUM(BlueprintType)
enum class EGameFlowPhase : uint8
{
	WaitingForPlayers UMETA(DisplayName = "Waiting For Players"),
	RoundStarting UMETA(DisplayName = "Round Starting"),
	RoundInProgress UMETA(DisplayName = "Round In Progress"),
	RoundEnding UMETA(DisplayName = "Round Ending"),
	MatchEnded UMETA(DisplayName = "Match Ended")
};

// Generic round results let objective modes report outcomes without coupling the base layer.
UENUM(BlueprintType)
enum class ERoundEndReason : uint8
{
	None UMETA(DisplayName = "None"),
	RoundTimeExpired UMETA(DisplayName = "Round Time Expired"),
	TeamEliminated UMETA(DisplayName = "Team Eliminated"),
	MutualElimination UMETA(DisplayName = "Mutual Elimination"),
	ObjectiveCompleted UMETA(DisplayName = "Objective Completed"),
	Forced UMETA(DisplayName = "Forced")
};
