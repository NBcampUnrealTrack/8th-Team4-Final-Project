#include "UI/InGame/HUD/DominationObjectiveFeedbackWidget.h"

#include "Engine/World.h"
#include "GameMode/State/DominationGameState.h"

void UDominationObjectiveFeedbackWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ADominationGameState* DominationGameState = GetWorld()
		? GetWorld()->GetGameState<ADominationGameState>()
		: nullptr;
	if (!IsValid(DominationGameState))
	{
		if (GetWorld() && GetWorld()->GetGameState())
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	BoundGameState = DominationGameState;
	DominationGameState->OnObjectiveFlowStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleObjectiveFlowStateChanged);
	RefreshObjectiveFeedback(true);
}

void UDominationObjectiveFeedbackWidget::NativeDestruct()
{
	if (ADominationGameState* DominationGameState = BoundGameState.Get())
	{
		DominationGameState->OnObjectiveFlowStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleObjectiveFlowStateChanged);
	}

	BoundGameState.Reset();
	Super::NativeDestruct();
}

FDominationObjectiveFlowState
UDominationObjectiveFeedbackWidget::GetObjectiveFlowState() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState)
		? DominationGameState->GetObjectiveFlowState()
		: FDominationObjectiveFlowState();
}

void UDominationObjectiveFeedbackWidget::HandleObjectiveFlowStateChanged()
{
	RefreshObjectiveFeedback(false);
}

void UDominationObjectiveFeedbackWidget::RefreshObjectiveFeedback(
	const bool bInitialBinding)
{
	ADominationGameState* DominationGameState = BoundGameState.Get();
	if (!IsValid(DominationGameState))
	{
		return;
	}

	const FDominationObjectiveFlowState FlowState =
		DominationGameState->GetObjectiveFlowState();
	const bool bIsTransitioning =
		FlowState.Phase == EDominationObjectivePhase::Repositioning;

	// HUD가 이미 끝난 전환 상태에서 늦게 생성된 경우 오래된 점령 알림을 재생하지 않는다.
	// 단, 아직 Repositioning 중이면 현재 전선 이동 정보는 유효하므로 그대로 보여준다.
	if (bInitialBinding && !bIsTransitioning)
	{
		LastHandledCaptureSerial = FlowState.CaptureSerial;
	}

	const bool bHasNewCapture = FlowState.CaptureSerial > 0
		&& FlowState.CaptureSerial != LastHandledCaptureSerial;
	if (bHasNewCapture)
	{
		const bool bHasNextObjective =
			FlowState.ActivePointId != EDominationPointId::None
			&& FlowState.ActivePointId != FlowState.LastCapturedPointId;
		const int32 TransitionDurationSeconds = bIsTransitioning
			? FMath::CeilToInt(FlowState.TransitionDuration)
			: 0;

		BP_OnObjectiveCaptured(
			FlowState.LastCapturingTeam,
			FlowState.LastCapturedPointId,
			FlowState.ActivePointId,
			bHasNextObjective,
			TransitionDurationSeconds);
		LastHandledCaptureSerial = FlowState.CaptureSerial;
	}

	if (bIsTransitioning)
	{
		if (!bTransitionPresented)
		{
			// CaptureSerial이 0인 Repositioning은 최초 중앙 거점 오픈 대기다.
			if (FlowState.CaptureSerial == 0
				&& FlowState.LastCapturedPointId == EDominationPointId::None)
			{
				BP_OnInitialObjectiveTransitionStarted(
					FlowState.ActivePointId,
					FMath::CeilToInt(FlowState.TransitionDuration));
			}

			bTransitionPresented = true;
		}

		return;
	}

	if (bTransitionPresented)
	{
		bTransitionPresented = false;
		BP_OnObjectiveTransitionEnded(FlowState.ActivePointId);
	}
}
