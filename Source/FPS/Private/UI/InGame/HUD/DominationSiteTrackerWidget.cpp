#include "UI/InGame/HUD/DominationSiteTrackerWidget.h"

#include "Actor/DominationCapturePoint.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/BaseTeamPlayerState.h"

void UDominationSiteTrackerWidget::NativeConstruct()
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
	DominationGameState->OnActivePointChanged.AddUniqueDynamic(this, &ThisClass::HandleActivePointChanged);
	DominationGameState->OnCaptureStateChanged.AddUniqueDynamic(this, &ThisClass::HandleCaptureStateChanged);
	DominationGameState->OnTeamScoreChanged.AddUniqueDynamic(this, &ThisClass::HandleTeamScoreChanged);
	DominationGameState->OnObjectiveFlowStateChanged.AddUniqueDynamic(this, &ThisClass::HandleObjectiveFlowStateChanged);

	// 위젯이 라운드 시작 이후에 만들어졌을 수 있으므로 현재 상태를 즉시 1회 반영
	HandleActivePointChanged(DominationGameState->GetActivePointId());
	HandleCaptureStateChanged(DominationGameState->GetCaptureState());
	BP_OnRoundScoreChanged(DominationGameState->GetTeamAScore(), DominationGameState->GetTeamBScore());
	RefreshRelativeScore(true);
	BP_OnTransitionStateChanged(IsPointTransitioning());

	GetWorld()->GetTimerManager().SetTimer(
		LocalCaptureZoneTimerHandle,
		this,
		&ThisClass::RefreshLocalState,
		LocalCaptureZoneUpdateInterval,
		true);
}

void UDominationSiteTrackerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (ADominationGameState* DominationGameState = BoundGameState.Get())
	{
		DominationGameState->OnActivePointChanged.RemoveDynamic(this, &ThisClass::HandleActivePointChanged);
		DominationGameState->OnCaptureStateChanged.RemoveDynamic(this, &ThisClass::HandleCaptureStateChanged);
		DominationGameState->OnTeamScoreChanged.RemoveDynamic(this, &ThisClass::HandleTeamScoreChanged);
		DominationGameState->OnObjectiveFlowStateChanged.RemoveDynamic(this, &ThisClass::HandleObjectiveFlowStateChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalCaptureZoneTimerHandle);
	}

	if (ADominationCapturePoint* CapturePoint = ActiveCapturePoint.Get())
	{
		CapturePoint->OnPawnEntered.RemoveAll(this);
		CapturePoint->OnPawnExited.RemoveAll(this);
	}

	ActiveCapturePoint.Reset();
	BoundGameState.Reset();
	Super::NativeDestruct();
}

EDominationPointId UDominationSiteTrackerWidget::GetActivePointId() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetActivePointId() : EDominationPointId::None;
}

int32 UDominationSiteTrackerWidget::GetActivePointIndex() const
{
	return DominationRules::GetPointIndex(GetActivePointId());
}

int32 UDominationSiteTrackerWidget::GetPointCount() const
{
	return DominationRules::PointCount;
}

TArray<EDominationPointId> UDominationSiteTrackerWidget::GetAllPointIds() const
{
	TArray<EDominationPointId> PointIds;
	PointIds.Reserve(DominationRules::PointCount);
	for (int32 Index = 0; Index < DominationRules::PointCount; ++Index)
	{
		PointIds.Add(DominationRules::GetPointId(Index));
	}

	return PointIds;
}

float UDominationSiteTrackerWidget::GetCaptureProgress() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetCaptureProgress() : 0.0f;
}

EGameTeam UDominationSiteTrackerWidget::GetCapturingTeam() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetCapturingTeam() : EGameTeam::None;
}

EDominationCaptureRelation UDominationSiteTrackerWidget::GetCapturingRelation() const
{
	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const EGameTeam LocalTeam = IsValid(LocalPlayerState) ? LocalPlayerState->GetTeam() : EGameTeam::None;

	return DominationRules::GetCaptureRelation(GetCapturingTeam(), LocalTeam);
}

bool UDominationSiteTrackerWidget::IsCaptureContested() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) && DominationGameState->IsCaptureContested();
}

int32 UDominationSiteTrackerWidget::GetTeamAScore() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetTeamAScore() : 0;
}

int32 UDominationSiteTrackerWidget::GetTeamBScore() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetTeamBScore() : 0;
}

bool UDominationSiteTrackerWidget::IsLocalPlayerInsideActivePoint() const
{
	return bLocalPlayerInsideActivePoint;
}

bool UDominationSiteTrackerWidget::IsPointTransitioning() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) && DominationGameState->IsPointTransitioning();
}

int32 UDominationSiteTrackerWidget::GetTransitionSecondsRemaining() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState)
		? FMath::Max(0, FMath::CeilToInt(DominationGameState->GetPointTransitionTimeRemaining()))
		: 0;
}

void UDominationSiteTrackerWidget::HandleActivePointChanged(const EDominationPointId ActivePointId)
{
	BP_OnActivePointChanged(ActivePointId, DominationRules::GetPointIndex(ActivePointId));

	RefreshActiveCapturePoint();
	RefreshLocalCaptureZoneState();
}

void UDominationSiteTrackerWidget::HandleCaptureStateChanged(const FDominationCaptureState CaptureState)
{
	BP_OnCaptureStateChanged(GetCapturingRelation(), CaptureState.CaptureProgress, CaptureState.bContested);
}

void UDominationSiteTrackerWidget::HandleTeamScoreChanged(const EGameTeam, const int32)
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	if (IsValid(DominationGameState))
	{
		BP_OnRoundScoreChanged(DominationGameState->GetTeamAScore(), DominationGameState->GetTeamBScore());
	}

	RefreshRelativeScore(true);
}

void UDominationSiteTrackerWidget::HandleObjectiveFlowStateChanged()
{
	BP_OnTransitionStateChanged(IsPointTransitioning());
}

void UDominationSiteTrackerWidget::HandleCapturePointPawnChanged(APawn*)
{
	RefreshLocalCaptureZoneState();
}

void UDominationSiteTrackerWidget::RefreshActiveCapturePoint()
{
	if (ADominationCapturePoint* PreviousCapturePoint = ActiveCapturePoint.Get())
	{
		PreviousCapturePoint->OnPawnEntered.RemoveAll(this);
		PreviousCapturePoint->OnPawnExited.RemoveAll(this);
	}
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
			Iterator->OnPawnEntered.AddUObject(this, &ThisClass::HandleCapturePointPawnChanged);
			Iterator->OnPawnExited.AddUObject(this, &ThisClass::HandleCapturePointPawnChanged);
			return;
		}
	}
}

void UDominationSiteTrackerWidget::RefreshLocalCaptureZoneState()
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

void UDominationSiteTrackerWidget::RefreshLocalState()
{
	RefreshLocalCaptureZoneState();
	// 위젯이 만들어질 때 내 팀이 아직 복제 전일 수 있어 도착하면 다시 보낸다
	RefreshRelativeScore(false);
}

void UDominationSiteTrackerWidget::RefreshRelativeScore(const bool bForce)
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	if (!IsValid(DominationGameState))
	{
		return;
	}

	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const EGameTeam LocalTeam = IsValid(LocalPlayerState) ? LocalPlayerState->GetTeam() : EGameTeam::None;
	if (!bForce && LocalTeam == CachedLocalTeam)
	{
		return;
	}
	CachedLocalTeam = LocalTeam;

	// 팀을 아직 모르면 TeamA 를 우리 팀 자리에 둔다
	const bool bLocalIsTeamB = LocalTeam == EGameTeam::TeamB;
	const int32 TeamAScore = DominationGameState->GetTeamAScore();
	const int32 TeamBScore = DominationGameState->GetTeamBScore();
	BP_OnRelativeScoreChanged(
		bLocalIsTeamB ? TeamBScore : TeamAScore,
		bLocalIsTeamB ? TeamAScore : TeamBScore);
}
