#include "UI/InGame/HUD/DominationCaptureProgressWidget.h"

#include "Actor/DominationCapturePoint.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameMode/DominationGameState.h"
#include "GameFramework/Pawn.h"
#include "Player/BaseTeamPlayerState.h"

void UDominationCaptureProgressWidget::NativeConstruct()
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
	DominationGameState->OnActivePointChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleActivePointChanged);
	DominationGameState->OnCaptureStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleCaptureStateChanged);
	DominationGameState->OnObjectiveFlowStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleObjectiveFlowStateChanged);
	RefreshActiveCapturePoint();
	RefreshBlueprint();
	RefreshLocalCaptureZoneState();
	GetWorld()->GetTimerManager().SetTimer(
		LocalCaptureZoneTimerHandle,
		this,
		&ThisClass::RefreshLocalCaptureZoneState,
		LocalCaptureZoneUpdateInterval,
		true);
}

void UDominationCaptureProgressWidget::NativeDestruct()
{
	if (ADominationGameState* DominationGameState = BoundGameState.Get())
	{
		DominationGameState->OnActivePointChanged.RemoveDynamic(
			this,
			&ThisClass::HandleActivePointChanged);
		DominationGameState->OnCaptureStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCaptureStateChanged);
		DominationGameState->OnObjectiveFlowStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleObjectiveFlowStateChanged);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalCaptureZoneTimerHandle);
	}

	ActiveCapturePoint.Reset();
	BoundGameState.Reset();
	Super::NativeDestruct();
}

EDominationPointId UDominationCaptureProgressWidget::GetActivePointId() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState)
		? DominationGameState->GetActivePointId()
		: EDominationPointId::None;
}

FDominationCaptureState UDominationCaptureProgressWidget::GetCaptureState() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState)
		? DominationGameState->GetCaptureState()
		: FDominationCaptureState();
}

float UDominationCaptureProgressWidget::GetCaptureProgress() const
{
	return GetCaptureState().CaptureProgress;
}

EGameTeam UDominationCaptureProgressWidget::GetCapturingTeam() const
{
	return GetCaptureState().CapturingTeam;
}

bool UDominationCaptureProgressWidget::IsCaptureContested() const
{
	return GetCaptureState().bContested;
}

void UDominationCaptureProgressWidget::HandleActivePointChanged(
	const EDominationPointId)
{
	RefreshActiveCapturePoint();
	RefreshBlueprint();
	RefreshLocalCaptureZoneState();
}

void UDominationCaptureProgressWidget::HandleCaptureStateChanged(
	const FDominationCaptureState)
{
	RefreshBlueprint();
}

void UDominationCaptureProgressWidget::HandleObjectiveFlowStateChanged()
{
	RefreshLocalCaptureZoneState();
}

void UDominationCaptureProgressWidget::RefreshBlueprint()
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	if (!IsValid(DominationGameState))
	{
		return;
	}

	BP_OnObjectiveStateChanged(
		DominationGameState->GetActivePointId(),
		DominationGameState->GetCaptureState());
}

void UDominationCaptureProgressWidget::RefreshActiveCapturePoint()
{
	ActiveCapturePoint.Reset();
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	UWorld* World = GetWorld();
	if (!IsValid(DominationGameState) || !IsValid(World))
	{
		return;
	}

	const EDominationPointId ActivePointId = DominationGameState->GetActivePointId();
	for (TActorIterator<ADominationCapturePoint> Iterator(World); Iterator; ++Iterator)
	{
		if (Iterator->GetPointId() == ActivePointId)
		{
			ActiveCapturePoint = *Iterator;
			return;
		}
	}
}

void UDominationCaptureProgressWidget::RefreshLocalCaptureZoneState()
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	const ADominationCapturePoint* CapturePoint = ActiveCapturePoint.Get();
	const APawn* OwningPawn = GetOwningPlayerPawn();
	const ABaseTeamPlayerState* PlayerState = IsValid(OwningPawn)
		? OwningPawn->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	const bool bShouldShow = IsValid(DominationGameState)
		&& DominationGameState->IsObjectiveCaptureEnabled()
		&& IsValid(CapturePoint)
		&& IsValid(PlayerState)
		&& PlayerState->IsAlive()
		&& CapturePoint->ContainsWorldLocation(OwningPawn->GetActorLocation());

	if (bHasLocalCaptureZoneState && bLocalPlayerInsideActivePoint == bShouldShow)
	{
		return;
	}

	bHasLocalCaptureZoneState = true;
	bLocalPlayerInsideActivePoint = bShouldShow;
	BP_OnLocalCaptureZoneChanged(bShouldShow);
}
