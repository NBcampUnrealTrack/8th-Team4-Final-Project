#include "GameMode/DominationGameState.h"

#include "Net/UnrealNetwork.h"

void ADominationGameState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADominationGameState, MatchFlowState);
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

void ADominationGameState::OnRep_MatchFlowState()
{
	BroadcastMatchFlowStateChanged();
}

void ADominationGameState::BroadcastMatchFlowStateChanged()
{
	OnDominationFlowStateChanged.Broadcast();
}
