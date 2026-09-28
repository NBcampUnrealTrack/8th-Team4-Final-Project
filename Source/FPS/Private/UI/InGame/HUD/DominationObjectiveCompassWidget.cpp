#include "UI/InGame/HUD/DominationObjectiveCompassWidget.h"

#include "Actor/DominationCapturePoint.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/BaseTeamPlayerState.h"

void UDominationObjectiveCompassWidget::NativeConstruct()
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
	DominationGameState->OnActivePointChanged.AddUniqueDynamic(this, &ThisClass::HandleActivePointChanged);
	DominationGameState->OnObjectiveFlowStateChanged.AddUniqueDynamic(this, &ThisClass::HandleObjectiveFlowStateChanged);
	DominationGameState->OnCaptureStateChanged.AddUniqueDynamic(this, &ThisClass::HandleCaptureStateChanged);
	RefreshTargetCapturePoint();
}

void UDominationObjectiveCompassWidget::NativeDestruct()
{
	if (ADominationGameState* DominationGameState = BoundGameState.Get())
	{
		DominationGameState->OnActivePointChanged.RemoveDynamic(this, &ThisClass::HandleActivePointChanged);
		DominationGameState->OnObjectiveFlowStateChanged.RemoveDynamic(this, &ThisClass::HandleObjectiveFlowStateChanged);
		DominationGameState->OnCaptureStateChanged.RemoveDynamic(this, &ThisClass::HandleCaptureStateChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	BoundGameState.Reset();
	TargetCapturePoint.Reset();
	Super::NativeDestruct();
}

void UDominationObjectiveCompassWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 마커 위치만 카메라에 따라 매 프레임 갱신한다. 목표가 없으면 아무것도 하지 않는다.
	if (TargetCapturePoint.IsValid() || bShouldShow)
	{
		RefreshScreenPlacement();
		ApplyMarkerToWidget();
	}
}

EDominationPointId UDominationObjectiveCompassWidget::GetTargetPointId() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetActivePointId() : EDominationPointId::None;
}

bool UDominationObjectiveCompassWidget::ShouldShowMarker() const
{
	return bShouldShow;
}

bool UDominationObjectiveCompassWidget::IsOnScreen() const
{
	return bIsOnScreen;
}

FVector2D UDominationObjectiveCompassWidget::GetScreenPosition() const
{
	return ScreenPosition;
}

float UDominationObjectiveCompassWidget::GetArrowRotationDegrees() const
{
	return ArrowRotationDegrees;
}

bool UDominationObjectiveCompassWidget::IsTransitioning() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) && DominationGameState->IsPointTransitioning();
}

int32 UDominationObjectiveCompassWidget::GetSecondsRemaining() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState)
		? FMath::Max(0, FMath::CeilToInt(DominationGameState->GetPointTransitionTimeRemaining()))
		: 0;
}

float UDominationObjectiveCompassWidget::GetDistanceMeters() const
{
	return DistanceMeters;
}

EDominationCaptureRelation UDominationObjectiveCompassWidget::GetCapturingRelation() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	if (!IsValid(DominationGameState))
	{
		return EDominationCaptureRelation::Neutral;
	}

	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const EGameTeam LocalTeam = IsValid(LocalPlayerState) ? LocalPlayerState->GetTeam() : EGameTeam::None;

	return DominationRules::GetCaptureRelation(DominationGameState->GetCapturingTeam(), LocalTeam);
}

float UDominationObjectiveCompassWidget::GetCaptureProgress() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetCaptureProgress() : 0.0f;
}

void UDominationObjectiveCompassWidget::HandleActivePointChanged(const EDominationPointId)
{
	RefreshTargetCapturePoint();
}

void UDominationObjectiveCompassWidget::HandleObjectiveFlowStateChanged()
{
	BP_OnCompassRefresh();
}

void UDominationObjectiveCompassWidget::HandleCaptureStateChanged(FDominationCaptureState)
{
	BP_OnCompassRefresh();
}

void UDominationObjectiveCompassWidget::HandleRefreshTimer()
{
	BP_OnCompassRefresh();
}

void UDominationObjectiveCompassWidget::UpdateRefreshTimer()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	// 거리·남은 초는 시간과 위치에 따라 바뀌므로 목표가 있는 동안만 주기적으로 갱신한다.
	FTimerManager& TimerManager = World->GetTimerManager();
	if (TargetCapturePoint.IsValid() && !TimerManager.IsTimerActive(RefreshTimerHandle))
	{
		TimerManager.SetTimer(RefreshTimerHandle, this, &ThisClass::HandleRefreshTimer, RefreshInterval, true);
	}
	else if (!TargetCapturePoint.IsValid())
	{
		TimerManager.ClearTimer(RefreshTimerHandle);
	}
}

void UDominationObjectiveCompassWidget::ApplyMarkerToWidget()
{
	if (!IsValid(Marker))
	{
		return;
	}

	const ESlateVisibility NewVisibility = bShouldShow
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	if (Marker->GetVisibility() != NewVisibility)
	{
		Marker->SetVisibility(NewVisibility);
	}

	if (bShouldShow)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
		{
			CanvasSlot->SetPosition(ScreenPosition);
		}
	}
}

void UDominationObjectiveCompassWidget::RefreshTargetCapturePoint()
{
	TargetCapturePoint.Reset();

	const ADominationGameState* DominationGameState = BoundGameState.Get();
	UWorld* World = GetWorld();
	const EDominationPointId TargetPointId = IsValid(DominationGameState)
		? DominationGameState->GetActivePointId()
		: EDominationPointId::None;

	// 목표가 없어도 아래 정리(타이머 해제, 마커 숨김, BP 갱신)는 항상 수행한다.
	if (IsValid(World) && TargetPointId != EDominationPointId::None)
	{
		for (TActorIterator<ADominationCapturePoint> Iterator(World); Iterator; ++Iterator)
		{
			if (Iterator->GetPointId() == TargetPointId)
			{
				TargetCapturePoint = *Iterator;
				break;
			}
		}
	}

	UpdateRefreshTimer();
	RefreshScreenPlacement();
	ApplyMarkerToWidget();
	BP_OnCompassRefresh();
}

void UDominationObjectiveCompassWidget::RefreshScreenPlacement()
{
	const ADominationCapturePoint* CapturePoint = TargetCapturePoint.Get();
	const UWorld* World = GetWorld();
	APlayerController* LocalController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	const APawn* LocalPawn = IsValid(LocalController) ? LocalController->GetPawn() : nullptr;

	if (!IsValid(CapturePoint) || !IsValid(LocalController) || !IsValid(LocalPawn))
	{
		bShouldShow = false;
		return;
	}

	// 목표 거점의 점령 구역 안에 있으면 표시하지 않는다.
	if (CapturePoint->ContainsWorldLocation(LocalPawn->GetActorLocation()))
	{
		bShouldShow = false;
		return;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	LocalController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		bShouldShow = false;
		return;
	}

	bShouldShow = true;
	DistanceMeters = FVector::Dist(CapturePoint->GetActorLocation(), LocalPawn->GetActorLocation()) / 100.0f;
	
	// 해상도에 따른 위치 고정
	const float ViewportScale = FMath::Max(KINDA_SMALL_NUMBER, UWidgetLayoutLibrary::GetViewportScale(this));
	const FVector2D LocalViewportSize(ViewportSizeX / ViewportScale, ViewportSizeY / ViewportScale);

	const FVector2D ScreenCenter(LocalViewportSize.X * 0.5f, LocalViewportSize.Y * 0.5f);
	const float SafeHalfWidth = FMath::Max(1.0f, LocalViewportSize.X * 0.5f - EdgePaddingPixels);
	const float SafeHalfHeight = FMath::Max(1.0f, LocalViewportSize.Y * 0.5f - EdgePaddingPixels);

	FVector2D ProjectedPosition;
	const bool bProjected = LocalController->ProjectWorldLocationToScreen(
		CapturePoint->GetActorLocation(), ProjectedPosition, true);

	if (bProjected)
	{
		ProjectedPosition /= ViewportScale;
		const FVector2D Offset = ProjectedPosition - ScreenCenter;
		if (FMath::Abs(Offset.X) <= SafeHalfWidth && FMath::Abs(Offset.Y) <= SafeHalfHeight)
		{
			bIsOnScreen = true;
			ScreenPosition = ProjectedPosition;
			ArrowRotationDegrees = 0.0f;
			return;
		}
	}

	// 카메라 기준 Yaw/Pitch 각도차로 방향만 계산해 화면 가장자리에 클램프
	bIsOnScreen = false;

	FVector CameraLocation;
	FRotator CameraRotation;
	LocalController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector DirectionToTarget = (CapturePoint->GetActorLocation() - CameraLocation).GetSafeNormal();
	const FRotator DeltaRotation = (DirectionToTarget.Rotation() - CameraRotation).GetNormalized();

	FVector2D Direction(DeltaRotation.Yaw, -DeltaRotation.Pitch);
	if (Direction.IsNearlyZero())
	{
		Direction = FVector2D(0.0f, -1.0f);
	}
	Direction.Normalize();

	const float ScaleX = SafeHalfWidth / FMath::Max(KINDA_SMALL_NUMBER, FMath::Abs(Direction.X));
	const float ScaleY = SafeHalfHeight / FMath::Max(KINDA_SMALL_NUMBER, FMath::Abs(Direction.Y));
	const float Scale = FMath::Min(ScaleX, ScaleY);

	ScreenPosition = ScreenCenter + Direction * Scale;
	ArrowRotationDegrees = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
}
