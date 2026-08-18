#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Common/TeamTypes.h"
#include "GameFlow/Round/RoundTypes.h"

/** 월드 상태와 무관하게 계산된 폭탄전 라운드 종료 결과다. */
struct FPS_API FBombRoundResolution
{
	EGameTeam WinningTeam = EGameTeam::None;
	ERoundEndReason EndReason = ERoundEndReason::None;

	bool ShouldEndRound() const
	{
		return WinningTeam != EGameTeam::None && EndReason != ERoundEndReason::None;
	}
};

/** 서버 GameMode가 수집한 현재 상태를 폭탄전 승패 규칙으로 판정한다. */
namespace BombRoundRules
{
	FPS_API FBombRoundResolution ResolveElimination(
		int32 AliveAttackers,
		int32 AliveDefenders,
		bool bObjectiveIsPlanted,
		EGameTeam Attackers,
		EGameTeam Defenders);

	FPS_API FBombRoundResolution ResolveTimeExpired(
		int32 AliveAttackers,
		int32 AliveDefenders,
		bool bObjectiveIsPlanted,
		EGameTeam Attackers,
		EGameTeam Defenders);
}
