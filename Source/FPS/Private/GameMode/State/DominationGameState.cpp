#include "GameMode/State/DominationGameState.h"

#include "Net/UnrealNetwork.h"

namespace
{
	bool AreCaptureStatesEqual(
		const FDominationCaptureState& Left,
		const FDominationCaptureState& Right)
	{
		return Left.CapturingTeam == Right.CapturingTeam
			&& FMath::IsNearlyEqual(Left.CaptureProgress, Right.CaptureProgress)
			&& Left.TeamAPlayerCount == Right.TeamAPlayerCount
			&& Left.TeamBPlayerCount == Right.TeamBPlayerCount
			&& Left.bContested == Right.bContested;
	}
}

void ADominationGameState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADominationGameState, MatchFlowState);
	DOREPLIFETIME(ADominationGameState, ObjectiveFlowState);
	DOREPLIFETIME(ADominationGameState, CaptureState);
}

EDominationMatchPhase ADominationGameState::GetDominationPhase() const
{
	return MatchFlowState.Phase;
}

float ADominationGameState::GetPhaseTimeRemaining() const
{
	const float RemainingTime = static_cast<float>(
		MatchFlowState.PhaseEndServerTime - GetServerWorldTimeSeconds());
	return FMath::Max(0.0f, RemainingTime);
}

int32 ADominationGameState::GetCompletedRoundCount() const
{
	return GetTeamAScore() + GetTeamBScore();
}

int32 ADominationGameState::GetCurrentRoundNumber() const
{
	const int32 CompletedRoundCount = GetCompletedRoundCount();
	const bool bWinningScoreReached = GetTeamAScore() >= GetRoundsToWin()
		|| GetTeamBScore() >= GetRoundsToWin();
	return bWinningScoreReached
		? CompletedRoundCount
		: CompletedRoundCount + 1;
}

int32 ADominationGameState::GetRoundsToWin() const
{
	return DominationRules::RoundsToWin;
}

int32 ADominationGameState::GetMaxRounds() const
{
	return DominationRules::MaxRounds;
}

EDominationPointId ADominationGameState::GetActivePointId() const
{
	return ObjectiveFlowState.ActivePointId;
}

bool ADominationGameState::HasActivePoint() const
{
	return ObjectiveFlowState.ActivePointId != EDominationPointId::None;
}

EDominationObjectivePhase ADominationGameState::GetObjectivePhase() const
{
	return ObjectiveFlowState.Phase;
}

bool ADominationGameState::IsObjectiveCaptureEnabled() const
{
	return ObjectiveFlowState.Phase == EDominationObjectivePhase::CaptureEnabled;
}

bool ADominationGameState::IsPointTransitioning() const
{
	return ObjectiveFlowState.Phase == EDominationObjectivePhase::Repositioning;
}

float ADominationGameState::GetPointTransitionTimeRemaining() const
{
	if (!IsPointTransitioning())
	{
		return 0.0f;
	}

	const float RemainingTime = static_cast<float>(
		ObjectiveFlowState.PhaseEndServerTime - GetServerWorldTimeSeconds());
	return FMath::Max(0.0f, RemainingTime);
}

FDominationObjectiveFlowState ADominationGameState::GetObjectiveFlowState() const
{
	return ObjectiveFlowState;
}

EDominationPointId ADominationGameState::GetLastCapturedPointId() const
{
	return ObjectiveFlowState.LastCapturedPointId;
}

EGameTeam ADominationGameState::GetLastCapturingTeam() const
{
	return ObjectiveFlowState.LastCapturingTeam;
}

int32 ADominationGameState::GetObjectiveCaptureSerial() const
{
	return ObjectiveFlowState.CaptureSerial;
}

FDominationCaptureState ADominationGameState::GetCaptureState() const
{
	return CaptureState;
}

float ADominationGameState::GetCaptureProgress() const
{
	return CaptureState.CaptureProgress;
}

EGameTeam ADominationGameState::GetCapturingTeam() const
{
	return CaptureState.CapturingTeam;
}

bool ADominationGameState::IsCaptureContested() const
{
	return CaptureState.bContested;
}

void ADominationGameState::SetDominationPhase(
	const EDominationMatchPhase NewPhase,
	const float PhaseDuration)
{
	if (!HasAuthority())
	{
		return;
	}

	MatchFlowState.Phase = NewPhase;
	MatchFlowState.PhaseEndServerTime = PhaseDuration > 0.0f
		? static_cast<float>(GetServerWorldTimeSeconds() + PhaseDuration)
		: 0.0f;
	BroadcastMatchFlowStateChanged();
	ForceNetUpdate();
}

void ADominationGameState::ResetObjectiveState()
{
	if (!HasAuthority())
	{
		return;
	}

	ObjectiveFlowState = FDominationObjectiveFlowState();
	OnRep_ObjectiveFlowState();
	ForceNetUpdate();
}

void ADominationGameState::BeginInitialObjectiveTransition(
	const EDominationPointId InitialPointId,
	const float PhaseDuration)
{
	ApplyObjectiveState(
		InitialPointId,
		EDominationObjectivePhase::Repositioning,
		PhaseDuration,
		EDominationPointId::None,
		EGameTeam::None,
		false);
}

void ADominationGameState::BeginObjectiveTransitionAfterCapture(
	const EDominationPointId CapturedPointId,
	const EGameTeam CapturingTeam,
	const EDominationPointId NextPointId,
	const float PhaseDuration)
{
	if (CapturedPointId == EDominationPointId::None
		|| CapturingTeam == EGameTeam::None
		|| NextPointId == EDominationPointId::None)
	{
		return;
	}

	ApplyObjectiveState(
		NextPointId,
		EDominationObjectivePhase::Repositioning,
		PhaseDuration,
		CapturedPointId,
		CapturingTeam,
		true);
}

void ADominationGameState::RecordObjectiveCapture(
	const EDominationPointId CapturedPointId,
	const EGameTeam CapturingTeam)
{
	if (!HasAuthority()
		|| CapturedPointId == EDominationPointId::None
		|| CapturingTeam == EGameTeam::None)
	{
		return;
	}

	ObjectiveFlowState.LastCapturedPointId = CapturedPointId;
	ObjectiveFlowState.LastCapturingTeam = CapturingTeam;
	++ObjectiveFlowState.CaptureSerial;
	OnRep_ObjectiveFlowState();
	ForceNetUpdate();
}

void ADominationGameState::SetObjectiveCaptureEnabled(
	const EDominationPointId ActivePointId)
{
	ApplyObjectiveState(
		ActivePointId,
		EDominationObjectivePhase::CaptureEnabled,
		0.0f,
		ObjectiveFlowState.LastCapturedPointId,
		ObjectiveFlowState.LastCapturingTeam,
		false);
}

void ADominationGameState::SetObjectiveInactive(
	const EDominationPointId ActivePointId)
{
	ApplyObjectiveState(
		ActivePointId,
		EDominationObjectivePhase::Inactive,
		0.0f,
		ObjectiveFlowState.LastCapturedPointId,
		ObjectiveFlowState.LastCapturingTeam,
		false);
}

void ADominationGameState::ApplyObjectiveState(
	const EDominationPointId NewActivePointId,
	const EDominationObjectivePhase NewPhase,
	const float PhaseDuration,
	const EDominationPointId LastCapturedPointId,
	const EGameTeam LastCapturingTeam,
	const bool bIncrementCaptureSerial)
{
	if (!HasAuthority())
	{
		return;
	}

	ObjectiveFlowState.ActivePointId = NewActivePointId;
	ObjectiveFlowState.Phase = NewPhase;
	ObjectiveFlowState.PhaseEndServerTime = PhaseDuration > 0.0f
		? static_cast<float>(GetServerWorldTimeSeconds() + PhaseDuration)
		: 0.0f;
	ObjectiveFlowState.TransitionDuration =
		NewPhase == EDominationObjectivePhase::Repositioning
			? FMath::Max(0.0f, PhaseDuration)
			: 0.0f;
	ObjectiveFlowState.LastCapturedPointId = LastCapturedPointId;
	ObjectiveFlowState.LastCapturingTeam = LastCapturingTeam;
	if (bIncrementCaptureSerial)
	{
		++ObjectiveFlowState.CaptureSerial;
	}
	OnRep_ObjectiveFlowState();
	ForceNetUpdate();
}

void ADominationGameState::SetCaptureState(
	const FDominationCaptureState& NewCaptureState)
{
	if (!HasAuthority() || AreCaptureStatesEqual(CaptureState, NewCaptureState))
	{
		return;
	}

	CaptureState = NewCaptureState;
	OnRep_CaptureState();
	ForceNetUpdate();
}

void ADominationGameState::OnRep_MatchFlowState()
{
	BroadcastMatchFlowStateChanged();
}

void ADominationGameState::OnRep_ObjectiveFlowState()
{
	OnActivePointChanged.Broadcast(ObjectiveFlowState.ActivePointId);
	OnObjectiveFlowStateChanged.Broadcast();
}

void ADominationGameState::OnRep_CaptureState()
{
	OnCaptureStateChanged.Broadcast(CaptureState);
}

void ADominationGameState::BroadcastMatchFlowStateChanged()
{
	OnDominationFlowStateChanged.Broadcast();
}
