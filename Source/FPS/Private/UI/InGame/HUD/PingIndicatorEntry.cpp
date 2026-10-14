// PingIndicatorEntry.cpp
#include "UI/InGame/HUD/PingIndicatorEntry.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UPingIndicatorEntry::InitializeIndicator(UCanvasPanel* InOwnerCanvas, UCanvasPanelSlot* InCanvasSlot)
{
	OwnerCanvas = InOwnerCanvas;
	CachedCanvasSlot = InCanvasSlot;
}

void UPingIndicatorEntry::Activate(const FPingEvent& Ping)
{
	PingLocation = Ping.Location;
	bActive = true;

	if (Image_LocationIcon)
	{
		Image_LocationIcon->SetVisibility(
			Ping.Type == EShooterPingType::Location ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Image_EnemyIcon)
	{
		Image_EnemyIcon->SetVisibility(
			Ping.Type == EShooterPingType::Enemy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// 첫 투영이 성공한 Tick에서만 실제로 보여준다(TeammateIndicatorEntry와 동일한 이유 -
	// 첫 Tick의 캔버스 지오메트리가 0이라 배치가 틀어질 수 있다).
	if (Panel_IndicatorVisual)
	{
		Panel_IndicatorVisual->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ExpiryTimerHandle, this, &ThisClass::HandleExpired, ShooterPingLimits::PingLifetimeSeconds, false);
	}
}

void UPingIndicatorEntry::HandleExpired()
{
	Deactivate();
}

void UPingIndicatorEntry::Deactivate()
{
	bActive = false;
	SetVisibility(ESlateVisibility::Collapsed);

	if (Panel_IndicatorVisual)
	{
		Panel_IndicatorVisual->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpiryTimerHandle);
	}
}

void UPingIndicatorEntry::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bActive)
	{
		UpdateScreenPosition();
	}
}

void UPingIndicatorEntry::UpdateScreenPosition()
{
	auto HideVisual = [this]()
	{
		if (Panel_IndicatorVisual)
		{
			Panel_IndicatorVisual->SetVisibility(ESlateVisibility::Collapsed);
		}
	};

	APlayerController* PC = GetOwningPlayer();
	if (IsValid(PC) == false || CachedCanvasSlot.IsValid() == false || OwnerCanvas.IsValid() == false)
	{
		HideVisual();
		return;
	}

	const FGeometry CanvasGeom = OwnerCanvas->GetCachedGeometry();
	if (CanvasGeom.GetLocalSize().IsNearlyZero())
	{
		HideVisual();
		return;
	}

	FVector2D ViewportLocalPos = FVector2D::ZeroVector;
	const bool bProjected =
		UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, PingLocation, ViewportLocalPos, false);

	// 카메라 뒤
	if (bProjected == false)
	{
		HideVisual();
		return;
	}

	// bProjected는 카메라 뒤만 걸러줘서, 카메라 앞인데 FOV 밖인 경우는 좌표 범위로 직접 거른다.
	const FGeometry ViewportGeom = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	const FVector2D ViewportLocalSize = ViewportGeom.GetLocalSize();
	if (ViewportLocalPos.X < 0.f || ViewportLocalPos.Y < 0.f
		|| ViewportLocalPos.X > ViewportLocalSize.X
		|| ViewportLocalPos.Y > ViewportLocalSize.Y)
	{
		HideVisual();
		return;
	}

	const FVector2D AbsolutePos = ViewportGeom.LocalToAbsolute(ViewportLocalPos);
	const FVector2D OurLocalPos = CanvasGeom.AbsoluteToLocal(AbsolutePos);

	if (!GetRenderTransform().Translation.Equals(OurLocalPos, 0.05f))
	{
		SetRenderTranslation(OurLocalPos);
	}

	if (Panel_IndicatorVisual)
	{
		Panel_IndicatorVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
