#include "GameMode/BombRoundRules.h"

namespace
{
	bool HasValidTeams(const EGameTeam Attackers, const EGameTeam Defenders)
	{
		return Attackers != EGameTeam::None
			&& Defenders != EGameTeam::None
			&& Attackers != Defenders;
	}
}

FBombRoundResolution BombRoundRules::ResolveElimination(
	const int32 AliveAttackers,
	const int32 AliveDefenders,
	const bool bObjectiveIsPlanted,
	const EGameTeam Attackers,
	const EGameTeam Defenders)
{
	if (!HasValidTeams(Attackers, Defenders) || AliveAttackers < 0 || AliveDefenders < 0)
	{
		return {};
	}

	// 프로젝트 규칙: 설치 여부와 관계없이 동시 전멸은 수비 승리다.
	if (AliveAttackers == 0 && AliveDefenders == 0)
	{
		return { Defenders, ERoundEndReason::MutualElimination };
	}

	if (AliveDefenders == 0)
	{
		return { Attackers, ERoundEndReason::TeamEliminated };
	}

	// 설치가 끝났다면 공격팀 전멸만으로는 라운드를 종료하지 않는다.
	if (AliveAttackers == 0 && !bObjectiveIsPlanted)
	{
		return { Defenders, ERoundEndReason::TeamEliminated };
	}

	return {};
}

FBombRoundResolution BombRoundRules::ResolveTimeExpired(
	const int32 AliveAttackers,
	const int32 AliveDefenders,
	const bool bObjectiveIsPlanted,
	const EGameTeam Attackers,
	const EGameTeam Defenders)
{
	if (!HasValidTeams(Attackers, Defenders) || AliveAttackers < 0 || AliveDefenders < 0)
	{
		return {};
	}

	// 설치/해체 중에는 라운드 제한 시간이 승패를 결정하지 않는다.
	if (bObjectiveIsPlanted)
	{
		return {};
	}

	const FBombRoundResolution EliminationResult = ResolveElimination(
		AliveAttackers,
		AliveDefenders,
		false,
		Attackers,
		Defenders);
	if (EliminationResult.ShouldEndRound())
	{
		return EliminationResult;
	}

	return { Defenders, ERoundEndReason::RoundTimeExpired };
}
