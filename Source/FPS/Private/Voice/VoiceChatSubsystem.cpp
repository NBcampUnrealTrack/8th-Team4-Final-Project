#include "Voice/VoiceChatSubsystem.h"

#include "Engine/World.h"
#include "GameMode/BaseRoundGameState.h"
#include "GameMode/RoundTypes.h"
#include "GameMode/TeamTypes.h"
#include "GameFramework/PlayerState.h"
#include "Player/BaseTeamPlayerState.h"

void UVoiceChatSubsystem::SetPlayerMuted(APlayerState* PlayerState, const bool bMuted)
{
	if (!IsValid(PlayerState))
	{
		return;
	}

	if (bMuted)
	{
		MutedPlayers.Add(PlayerState);
		return;
	}

	for (auto It = MutedPlayers.CreateIterator(); It; ++It)
	{
		APlayerState* MutedPlayer = It->Get();
		if (!IsValid(MutedPlayer) || MutedPlayer == PlayerState)
		{
			It.RemoveCurrent();
		}
	}
}

void UVoiceChatSubsystem::TogglePlayerMuted(APlayerState* PlayerState)
{
	if (!IsValid(PlayerState))
	{
		return;
	}

	SetPlayerMuted(PlayerState, !IsPlayerMuted(PlayerState));
}

bool UVoiceChatSubsystem::IsPlayerMuted(const APlayerState* PlayerState) const
{
	if (!IsValid(PlayerState))
	{
		return false;
	}

	for (auto It = MutedPlayers.CreateIterator(); It; ++It)
	{
		const APlayerState* MutedPlayer = It->Get();
		if (!IsValid(MutedPlayer))
		{
			It.RemoveCurrent();
			continue;
		}

		if (MutedPlayer == PlayerState)
		{
			return true;
		}
	}

	return false;
}

bool UVoiceChatSubsystem::CanSendVoice(const APlayerState* Speaker) const
{
	if (!IsValid(Speaker))
	{
		return false;
	}

	if (!IsTeamVoiceOnly(Speaker->GetWorld()))
	{
		return true;
	}

	const ABaseTeamPlayerState* TeamSpeaker = Cast<ABaseTeamPlayerState>(Speaker);
	return IsValid(TeamSpeaker) && TeamSpeaker->GetTeam() != EGameTeam::None;
}

bool UVoiceChatSubsystem::CanHearVoice(const APlayerState* Listener, const APlayerState* Speaker) const
{
	if (!IsValid(Listener) || !IsValid(Speaker) || Listener == Speaker)
	{
		return false;
	}

	if (IsPlayerMuted(Speaker) || !CanSendVoice(Speaker))
	{
		return false;
	}

	if (!IsTeamVoiceOnly(Listener->GetWorld()))
	{
		return true;
	}

	return ArePlayersOnSameTeam(Listener, Speaker);
}

bool UVoiceChatSubsystem::IsTeamVoiceOnly(const UWorld* World) const
{
	if (!IsValid(World))
	{
		return false;
	}

	const ABaseRoundGameState* RoundGameState = World->GetGameState<ABaseRoundGameState>();
	if (!IsValid(RoundGameState))
	{
		return false;
	}

	switch (RoundGameState->GetGameFlowPhase())
	{
	case EGameFlowPhase::WaitingForPlayers:
	case EGameFlowPhase::WaitingForPlayersReady:
	case EGameFlowPhase::MatchEnded:
		return false;
	default:
		return true;
	}
}

bool UVoiceChatSubsystem::ArePlayersOnSameTeam(const APlayerState* Listener, const APlayerState* Speaker) const
{
	const ABaseTeamPlayerState* ListenerTeamState = Cast<ABaseTeamPlayerState>(Listener);
	const ABaseTeamPlayerState* SpeakerTeamState = Cast<ABaseTeamPlayerState>(Speaker);
	if (!IsValid(ListenerTeamState) || !IsValid(SpeakerTeamState))
	{
		return false;
	}

	const EGameTeam ListenerTeam = ListenerTeamState->GetTeam();
	return ListenerTeam != EGameTeam::None && ListenerTeam == SpeakerTeamState->GetTeam();
}
