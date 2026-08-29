#include "GameMode/BaseRoundGameMode.h"

#include "GameMode/BaseRoundGameState.h"
#include "GameMode/SurrenderVoteTypes.h"

bool ABaseRoundGameMode::IsSurrenderVoteAllowedNow() const
{
	if (!Super::IsSurrenderVoteAllowedNow())
	{
		return false;
	}

	const ABaseRoundGameState* RoundGameState = GetBaseRoundGameState();
	return IsValid(RoundGameState)
		&& RoundGameState->GetGameFlowPhase() != EGameFlowPhase::MatchEnded;
}

void ABaseRoundGameMode::HandleSurrenderApproved(const EGameTeam SurrenderingTeam)
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
