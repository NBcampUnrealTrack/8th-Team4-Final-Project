#include "GameMode/BaseRoundGameState.h"

#include "Net/UnrealNetwork.h"

void ABaseRoundGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseRoundGameState, CurrentRoundNumber);
	DOREPLIFETIME(ABaseRoundGameState, GameFlowPhase);
	DOREPLIFETIME(ABaseRoundGameState, RoundWinner);
	DOREPLIFETIME(ABaseRoundGameState, RoundEndReason);
	DOREPLIFETIME(ABaseRoundGameState, TeamARole);
	DOREPLIFETIME(ABaseRoundGameState, TeamBRole);
	DOREPLIFETIME(ABaseRoundGameState, RoundEndServerTime);
	DOREPLIFETIME(ABaseRoundGameState, FrozenRoundTimeRemaining);
	DOREPLIFETIME(ABaseRoundGameState, bRoundTimerFrozen);
	DOREPLIFETIME(ABaseRoundGameState, PhaseEndServerTime);
	DOREPLIFETIME(ABaseRoundGameState, MaxRounds);
	DOREPLIFETIME(ABaseRoundGameState, LoadedPlayerCount);
	DOREPLIFETIME(ABaseRoundGameState, RequiredPlayerCount);
	DOREPLIFETIME(ABaseRoundGameState, LoadingProgress);
}

int32 ABaseRoundGameState::GetCurrentRoundNumber() const
{
	return CurrentRoundNumber;
}

EGameFlowPhase ABaseRoundGameState::GetGameFlowPhase() const
{
	return GameFlowPhase;
}

EGameTeam ABaseRoundGameState::GetRoundWinner() const
{
	return RoundWinner;
}

ERoundEndReason ABaseRoundGameState::GetRoundEndReason() const
{
	return RoundEndReason;
}

ETeamRole ABaseRoundGameState::GetTeamRole(const EGameTeam Team) const
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

EGameTeam ABaseRoundGameState::GetTeamForRole(const ETeamRole TeamRole) const
{
	if (TeamARole == TeamRole)
	{
		return EGameTeam::TeamA;
	}

	if (TeamBRole == TeamRole)
	{
		return EGameTeam::TeamB;
	}

	return EGameTeam::None;
}

float ABaseRoundGameState::GetRoundTimeRemaining() const
{
	if (bRoundTimerFrozen)
	{
		return FMath::Max(0.0f, FrozenRoundTimeRemaining);
	}

	return FMath::Max(0.0f, RoundEndServerTime - GetServerWorldTimeSeconds());
}

bool ABaseRoundGameState::IsRoundTimerFrozen() const
{
	return bRoundTimerFrozen;
}

float ABaseRoundGameState::GetPhaseTimeRemaining() const
{
	return FMath::Max(0.0f, PhaseEndServerTime - GetServerWorldTimeSeconds());
}

int32 ABaseRoundGameState::GetLoadedPlayerCount() const
{
	return LoadedPlayerCount;
}

int32 ABaseRoundGameState::GetRequiredPlayerCount() const
{
	return RequiredPlayerCount;
}

float ABaseRoundGameState::GetLoadingProgress() const
{
	return LoadingProgress;
}

int32 ABaseRoundGameState::GetMaxRounds() const
{
	return MaxRounds;
}

void ABaseRoundGameState::InitializeRoundConfiguration(const int32 NewMaxRounds)
{
	if (!HasAuthority())
	{
		return;
	}

	MaxRounds = FMath::Max(0, NewMaxRounds);
	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::SetRoundContext(const FRoundContext& RoundContext)
{
	if (!HasAuthority() || !RoundContext.IsValid())
	{
		return;
	}

	CurrentRoundNumber = RoundContext.RoundNumber;
	TeamARole = RoundContext.TeamARole;
	TeamBRole = RoundContext.TeamBRole;
	RoundWinner = EGameTeam::None;
	RoundEndReason = ERoundEndReason::None;
	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::SetCurrentRoundNumber(const int32 NewRoundNumber)
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentRoundNumber = FMath::Max(0, NewRoundNumber);
	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::SetGameFlowPhase(const EGameFlowPhase NewPhase, const float PhaseDuration)
{
	if (!HasAuthority())
	{
		return;
	}

	GameFlowPhase = NewPhase;
	PhaseEndServerTime = PhaseDuration > 0.0f ? GetServerWorldTimeSeconds() + PhaseDuration : 0.0f;
	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::SetRoundTimer(const float Duration)
{
	if (!HasAuthority())
	{
		return;
	}

	bRoundTimerFrozen = false;
	FrozenRoundTimeRemaining = 0.0f;
	RoundEndServerTime = Duration > 0.0f ? GetServerWorldTimeSeconds() + Duration : 0.0f;
	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::FreezeRoundTimer()
{
	if (!HasAuthority() || bRoundTimerFrozen)
	{
		return;
	}

	FrozenRoundTimeRemaining = FMath::Max(0.0f, RoundEndServerTime - GetServerWorldTimeSeconds());
	RoundEndServerTime = 0.0f;
	bRoundTimerFrozen = true;
	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::ClearRoundTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	RoundEndServerTime = 0.0f;
	FrozenRoundTimeRemaining = 0.0f;
	bRoundTimerFrozen = false;
	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::SetRoundResult(const EGameTeam WinningTeam, const ERoundEndReason EndReason)
{
	if (!HasAuthority())
	{
		return;
	}

	RoundWinner = WinningTeam;
	RoundEndReason = EndReason;
	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::ClearRoundResult()
{
	if (!HasAuthority())
	{
		return;
	}

	RoundWinner = EGameTeam::None;
	RoundEndReason = ERoundEndReason::None;
	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::SetTeamRoles(const ETeamRole NewTeamARole, const ETeamRole NewTeamBRole)
{
	if (!HasAuthority())
	{
		return;
	}

	TeamARole = NewTeamARole;
	TeamBRole = NewTeamBRole;
	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::SetLoadingStatus(const int32 NewLoadedPlayerCount, const int32 NewRequiredPlayerCount)
{
	if (!HasAuthority())
	{
		return;
	}

	LoadedPlayerCount = FMath::Max(0, NewLoadedPlayerCount);
	RequiredPlayerCount = FMath::Max(0, NewRequiredPlayerCount);
	LoadingProgress = RequiredPlayerCount > 0
		? FMath::Clamp(static_cast<float>(LoadedPlayerCount) / static_cast<float>(RequiredPlayerCount), 0.0f, 1.0f)
		: 0.0f;

	BroadcastRoundStateChanged();
}

void ABaseRoundGameState::ClearLoadingStatus()
{
	SetLoadingStatus(0, 0);
}

void ABaseRoundGameState::HandleMatchStateChanged()
{
	Super::HandleMatchStateChanged();
	OnRoundStateChanged.Broadcast();
}

void ABaseRoundGameState::OnRep_RoundState()
{
	OnRoundStateChanged.Broadcast();
}

void ABaseRoundGameState::BroadcastRoundStateChanged()
{
	OnRoundStateChanged.Broadcast();
	ForceNetUpdate();
}
