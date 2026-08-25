#include "Voice/VoiceChatSubsystem.h"

#include "Engine/World.h"
#include "GameMode/BaseRoundGameState.h"
#include "GameMode/RoundTypes.h"
#include "GameMode/TeamTypes.h"
#include "GameFramework/PlayerState.h"
#include "Player/BaseTeamPlayerState.h"

static FString VoiceAudienceToString(const EVoiceChatAudience Audience)
{
	if (const UEnum* AudienceEnum = StaticEnum<EVoiceChatAudience>())
	{
		return AudienceEnum->GetNameStringByValue(static_cast<int64>(Audience));
	}

	return FString::FromInt(static_cast<int32>(Audience));
}

void UVoiceChatSubsystem::SetPlayerMuted(APlayerState* PlayerState, const bool bMuted)
{
	if (!IsValid(PlayerState))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Policy] SetPlayerMuted blocked: PlayerState=null Muted=%s"),
			bMuted ? TEXT("true") : TEXT("false"));
		return;
	}

	const bool bWasMuted = IsPlayerMuted(PlayerState);
	if (bWasMuted == bMuted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Policy] SetPlayerMuted unchanged. Player=%s Muted=%s"),
			*GetNameSafe(PlayerState),
			bMuted ? TEXT("true") : TEXT("false"));
		return;
	}

	if (bMuted)
	{
		MutedPlayers.Add(PlayerState);
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Policy] Muted player. Player=%s"), *GetNameSafe(PlayerState));
		OnVoiceMuteChanged.Broadcast(PlayerState, true);
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

	OnVoiceMuteChanged.Broadcast(PlayerState, false);
	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Policy] Unmuted player. Player=%s"), *GetNameSafe(PlayerState));
}

void UVoiceChatSubsystem::TogglePlayerMuted(APlayerState* PlayerState)
{
	if (!IsValid(PlayerState))
	{
		return;
	}

	SetPlayerMuted(PlayerState, !IsPlayerMuted(PlayerState));
}

void UVoiceChatSubsystem::SetVoiceAudience(const EVoiceChatAudience NewAudience)
{
	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Policy] VoiceAudience changed. Old=%s New=%s"),
		*VoiceAudienceToString(VoiceAudience),
		*VoiceAudienceToString(NewAudience));
	VoiceAudience = NewAudience;
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

bool UVoiceChatSubsystem::CanSendVoice(const APlayerState* Speaker, const EVoiceChatAudience Audience) const
{
	if (!IsValid(Speaker))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Policy] CanSendVoice=false Speaker=null Audience=%s"),
			*VoiceAudienceToString(Audience));
		return false;
	}

	if (GetEffectiveVoiceAudience(Speaker->GetWorld(), Audience) == EVoiceChatAudience::Everyone)
	{
		return true;
	}

	const ABaseTeamPlayerState* TeamSpeaker = Cast<ABaseTeamPlayerState>(Speaker);
	const bool bCanSendVoice = IsValid(TeamSpeaker) && TeamSpeaker->GetTeam() != EGameTeam::None;
	if (!bCanSendVoice)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Policy] CanSendVoice=false Speaker=%s TeamState=%s Audience=%s"),
			*GetNameSafe(Speaker),
			IsValid(TeamSpeaker) ? TEXT("valid") : TEXT("null"),
			*VoiceAudienceToString(Audience));
	}
	return bCanSendVoice;
}

bool UVoiceChatSubsystem::CanSendVoice(const APlayerState* Speaker) const
{
	return CanSendVoice(Speaker, VoiceAudience);
}

bool UVoiceChatSubsystem::CanHearVoice(
	const APlayerState* Listener,
	const APlayerState* Speaker,
	const EVoiceChatAudience Audience) const
{
	if (!IsValid(Listener) || !IsValid(Speaker) || Listener == Speaker)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Policy] CanHearVoice=false invalid/self. Listener=%s Speaker=%s Audience=%s"),
			*GetNameSafe(Listener),
			*GetNameSafe(Speaker),
			*VoiceAudienceToString(Audience));
		return false;
	}

	if (IsPlayerMuted(Speaker))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Policy] CanHearVoice=false muted. Listener=%s Speaker=%s Audience=%s"),
			*GetNameSafe(Listener),
			*GetNameSafe(Speaker),
			*VoiceAudienceToString(Audience));
		return false;
	}

	if (!CanSendVoice(Speaker, Audience))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Policy] CanHearVoice=false speaker cannot send. Listener=%s Speaker=%s Audience=%s"),
			*GetNameSafe(Listener),
			*GetNameSafe(Speaker),
			*VoiceAudienceToString(Audience));
		return false;
	}

	if (GetEffectiveVoiceAudience(Listener->GetWorld(), Audience) == EVoiceChatAudience::Everyone)
	{
		return true;
	}

	return ArePlayersOnSameTeam(Listener, Speaker);
}

bool UVoiceChatSubsystem::CanHearVoice(const APlayerState* Listener, const APlayerState* Speaker) const
{
	return CanHearVoice(Listener, Speaker, VoiceAudience);
}

EVoiceChatAudience UVoiceChatSubsystem::GetEffectiveVoiceAudience(
	const UWorld* World,
	const EVoiceChatAudience RequestedAudience) const
{
	if (!IsValid(World))
	{
		return RequestedAudience;
	}

	const ABaseRoundGameState* RoundGameState = World->GetGameState<ABaseRoundGameState>();
	if (!IsValid(RoundGameState))
	{
		return RequestedAudience;
	}

	switch (RoundGameState->GetGameFlowPhase())
	{
	case EGameFlowPhase::WaitingForPlayers:
	case EGameFlowPhase::WaitingForPlayersReady:
	case EGameFlowPhase::MatchEnded:
		return EVoiceChatAudience::Everyone;
	default:
		return RequestedAudience;
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
