#pragma once

#include "CoreMinimal.h"
#include "GameMode/TeamTypes.h"
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
	MatchEnded UMETA(DisplayName = "Match Ended"),
	// 기존 enum 값(0~4)을 보존해 기존 Blueprint/직렬화 자산과의 호환성을 유지한다.
	AugmentSelecting UMETA(DisplayName = "Augment Selecting"),
	// 맵 진입 직후 UI와 플레이어 준비가 끝나기 전에는 라운드를 열지 않는다.
	WaitingForPlayersReady UMETA(DisplayName = "Waiting For Players Ready")
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
	Forced UMETA(DisplayName = "Forced"),
	Surrender UMETA(DisplayName = "Surrender")
};

/** 서버 준비 단계에서만 사용하는 다음 라운드 정보. 준비가 끝나면 GameState에 공개한다. */
struct FRoundContext
{
	int32 RoundNumber = 0;
	ETeamRole TeamARole = ETeamRole::None;
	ETeamRole TeamBRole = ETeamRole::None;

	static FRoundContext Make(
		const int32 InRoundNumber,
		const int32 SideSwitchRound,
		const ETeamRole StartingTeamARole,
		const ETeamRole StartingTeamBRole)
	{
		FRoundContext Context;
		Context.RoundNumber = InRoundNumber;

		const bool bUseSwappedRoles = SideSwitchRound > 0 && InRoundNumber >= SideSwitchRound;
		Context.TeamARole = bUseSwappedRoles ? StartingTeamBRole : StartingTeamARole;
		Context.TeamBRole = bUseSwappedRoles ? StartingTeamARole : StartingTeamBRole;
		return Context;
	}

	bool IsValid() const
	{
		return RoundNumber > 0
			&& TeamARole != ETeamRole::None
			&& TeamBRole != ETeamRole::None
			&& TeamARole != TeamBRole;
	}

	ETeamRole GetRoleForTeam(const EGameTeam Team) const
	{
		switch (Team)
		{
		case EGameTeam::TeamA:
			return TeamARole;
		case EGameTeam::TeamB:
			return TeamBRole;
		default:
			return ETeamRole::None;
		}
	}

	EGameTeam GetTeamForRole(const ETeamRole Role) const
	{
		if (TeamARole == Role)
		{
			return EGameTeam::TeamA;
		}

		if (TeamBRole == Role)
		{
			return EGameTeam::TeamB;
		}

		return EGameTeam::None;
	}
};
