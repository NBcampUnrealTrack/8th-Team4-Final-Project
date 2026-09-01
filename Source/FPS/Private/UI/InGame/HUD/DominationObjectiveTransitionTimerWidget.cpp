#include "UI/InGame/HUD/DominationObjectiveTransitionTimerWidget.h"

#include "Engine/World.h"
#include "GameMode/State/DominationGameState.h"

void UDominationObjectiveTransitionTimerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ADominationGameState* DominationGameState = GetWorld()
		? GetWorld()->GetGameState<ADominationGameState>()
		: nullptr;
	if (!IsValid(DominationGameState))
	{
		return;
	}

	BoundGameState = DominationGameState;
	DominationGameState->OnObjectiveFlowStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleObjectiveFlowStateChanged);
	RefreshTransitionState();
}

void UDominationObjectiveTransitionTimerWidget::NativeDestruct()
{
	if (ADominationGameState* DominationGameState = BoundGameState.Get())
	{
		DominationGameState->OnObjectiveFlowStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleObjectiveFlowStateChanged);
	}

	StopLocalUpdateTimer();
	BoundGameState.Reset();
	Super::NativeDestruct();
}

float UDominationObjectiveTransitionTimerWidget::GetTransitionTimeRemaining() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState)
		? DominationGameState->GetPointTransitionTimeRemaining()
		: 0.0f;
}

void UDominationObjectiveTransitionTimerWidget::HandleObjectiveFlowStateChanged()
{
	RefreshTransitionState();
}

void UDominationObjectiveTransitionTimerWidget::RefreshTransitionState()
{
	ADominationGameState* DominationGameState = BoundGameState.Get();
	if (!IsValid(DominationGameState))
	{
		return;
	}

	const FDominationObjectiveFlowState FlowState =
		DominationGameState->GetObjectiveFlowState();
	const bool bShouldShow =
		FlowState.Phase == EDominationObjectivePhase::Repositioning
		&& FlowState.ActivePointId != EDominationPointId::None;

	if (!bShouldShow)
	{
		if (bTimerPresented)
		{
			const EDominationPointId EndedPointId = PresentedPointId;
			bTimerPresented = false;
			PresentedPointId = EDominationPointId::None;
			LastPresentedSeconds = INDEX_NONE;
			StopLocalUpdateTimer();
			BP_OnTransitionTimerEnded(EndedPointId);
		}
		return;
	}

	const bool bNewTransition = !bTimerPresented
		|| PresentedPointId != FlowState.ActivePointId;
	PresentedPointId = FlowState.ActivePointId;

	if (bNewTransition)
	{
		bTimerPresented = true;
		LastPresentedSeconds = INDEX_NONE;
		const int32 InitialSeconds = FMath::Max(
			0,
			FMath::CeilToInt(DominationGameState->GetPointTransitionTimeRemaining()));
		LastPresentedSeconds = InitialSeconds;
		BP_OnTransitionTimerStarted(PresentedPointId, InitialSeconds);
	}

	RefreshCountdown();
	StartLocalUpdateTimer();
}

void UDominationObjectiveTransitionTimerWidget::RefreshCountdown()
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	if (!bTimerPresented
		|| !IsValid(DominationGameState)
		|| !DominationGameState->IsPointTransitioning())
	{
		return;
	}

	const int32 SecondsRemaining = FMath::Max(
		0,
		FMath::CeilToInt(DominationGameState->GetPointTransitionTimeRemaining()));
	if (SecondsRemaining == LastPresentedSeconds)
	{
		return;
	}

	LastPresentedSeconds = SecondsRemaining;
	BP_OnTransitionTimerUpdated(PresentedPointId, SecondsRemaining);
}

void UDominationObjectiveTransitionTimerWidget::StartLocalUpdateTimer()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| World->GetTimerManager().IsTimerActive(CountdownUpdateTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		CountdownUpdateTimerHandle,
		this,
		&ThisClass::RefreshCountdown,
		CountdownUpdateInterval,
		true);
}

void UDominationObjectiveTransitionTimerWidget::StopLocalUpdateTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownUpdateTimerHandle);
	}
}
