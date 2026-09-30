// ShooterMapWidget.cpp
#include "UI/InGame/Map/ShooterMapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Minimap/ShooterMinimapBounds.h"
#include "Minimap/ShooterMinimapSubsystem.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "TextureResource.h"

namespace ShooterMapWidgetRules
{
	// 겹칠 때 위에 그려질 순서. 배경은 0.
	constexpr int32 EnemyZOrder = 10;
	constexpr int32 TeammateZOrder = 20;
	constexpr int32 PingZOrder = 25;
	constexpr int32 SelfZOrder = 30;

	constexpr float DefaultMarkerSize = 12.f;

	// 화살표는 방향이 보여야 해서 점보다 크게 잡는다.
	constexpr float DefaultArrowMarkerSize = 20.f;

	void InitMarkerBrush(FSlateBrush& Brush, const FLinearColor& Tint, const float Size = DefaultMarkerSize)
	{
		Brush.ImageSize = FVector2D(Size);
		Brush.TintColor = FSlateColor(Tint);
	}

	float DistanceToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double T = FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / AB.SizeSquared(), 0.0, 1.0);
		return FVector2D::Distance(Point, A + AB * T);
	}

	bool IsInsidePolygon(const FVector2D& Point, const TArrayView<const FVector2D> Polygon)
	{
		bool bInside = false;
		for (int32 Index = 0, Prev = Polygon.Num() - 1; Index < Polygon.Num(); Prev = Index++)
		{
			const FVector2D& A = Polygon[Index];
			const FVector2D& B = Polygon[Prev];
			if ((A.Y > Point.Y) != (B.Y > Point.Y)
				&& Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	// 위쪽을 가리키는 굵은 내비게이션 화살표(꼬리가 V자로 파인 삼각형). 흰색 몸통 + 어두운 테두리.
	// 몸통이 흰색이라 브러시 Tint 색이 그대로 입혀지고, 테두리는 어두워서 어떤 배경에서도 윤곽이 보인다.
	// 4x4 슈퍼샘플링으로 가장자리를 부드럽게 만든다.
	UTexture2D* CreateArrowTexture(const int32 Size)
	{
		const FVector2D Arrow[] = {
			FVector2D(0.50, 0.04),	// 끝
			FVector2D(0.90, 0.94),	// 오른쪽 뒤
			FVector2D(0.50, 0.72),	// 꼬리 파인 곳
			FVector2D(0.10, 0.94),	// 왼쪽 뒤
		};
		const float OutlineWidth = 0.07f;
		constexpr int32 Samples = 4;

		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		if (Texture == nullptr)
		{
			return nullptr;
		}

		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(Size * Size);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				float FillCoverage = 0.f;
				float OutlineCoverage = 0.f;
				for (int32 SY = 0; SY < Samples; ++SY)
				{
					for (int32 SX = 0; SX < Samples; ++SX)
					{
						const FVector2D Point((X + (SX + 0.5) / Samples) / Size, (Y + (SY + 0.5) / Samples) / Size);

						float EdgeDistance = TNumericLimits<float>::Max();
						for (int32 Index = 0; Index < UE_ARRAY_COUNT(Arrow); ++Index)
						{
							EdgeDistance = FMath::Min(EdgeDistance,
								DistanceToSegment(Point, Arrow[Index], Arrow[(Index + 1) % UE_ARRAY_COUNT(Arrow)]));
						}

						// 안쪽 가장자리 띠는 테두리, 그 안은 몸통. 바깥으로 삐져나가지 않게 테두리를 안쪽에 둔다.
						if (IsInsidePolygon(Point, Arrow))
						{
							if (EdgeDistance <= OutlineWidth)
							{
								OutlineCoverage += 1.f;
							}
							else
							{
								FillCoverage += 1.f;
							}
						}
					}
				}

				constexpr float SampleCount = Samples * Samples;
				const float Alpha = (FillCoverage + OutlineCoverage) / SampleCount;
				const float Brightness = Alpha > 0.f ? FillCoverage / (FillCoverage + OutlineCoverage) : 0.f;
				const uint8 Gray = static_cast<uint8>(FMath::Lerp(20.f, 255.f, Brightness));
				Pixels[Y * Size + X] = FColor(Gray, Gray, Gray, static_cast<uint8>(Alpha * 255.f));
			}
		}

		Texture->SRGB = true;
		Texture->Filter = TF_Bilinear;
		Texture->LODGroup = TEXTUREGROUP_UI;

		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		void* MipData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(MipData, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
		Mip.BulkData.Unlock();
		Texture->UpdateResource();

		return Texture;
	}
}

UShooterMapWidget::UShooterMapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 텍스처를 안 넣어도 색 사각형으로 바로 테스트할 수 있게 기본 틴트만 잡아둔다.
	ShooterMapWidgetRules::InitMarkerBrush(SelfMarkerBrush, FLinearColor(0.25f, 1.f, 0.45f), ShooterMapWidgetRules::DefaultArrowMarkerSize);
	ShooterMapWidgetRules::InitMarkerBrush(TeammateMarkerBrush, FLinearColor(0.2f, 0.65f, 1.f), ShooterMapWidgetRules::DefaultArrowMarkerSize);
	ShooterMapWidgetRules::InitMarkerBrush(EnemyMarkerBrush, FLinearColor(1.f, 0.1f, 0.1f));
	ShooterMapWidgetRules::InitMarkerBrush(PingLocationBrush, FLinearColor(1.f, 0.85f, 0.1f));
	ShooterMapWidgetRules::InitMarkerBrush(PingEnemyBrush, FLinearColor(1.f, 0.35f, 0.1f));
}

void UShooterMapWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ApplyLayoutToSlots();
	ApplyGeneratedArrowBrushes();
}

void UShooterMapWidget::ApplyGeneratedArrowBrushes()
{
	TArray<FSlateBrush*, TInlineAllocator<2>> ArrowBrushes;
	if (SelfMarkerBrush.GetResourceObject() == nullptr)
	{
		ArrowBrushes.Add(&SelfMarkerBrush);
	}
	if (bUseArrowForTeammates && TeammateMarkerBrush.GetResourceObject() == nullptr)
	{
		ArrowBrushes.Add(&TeammateMarkerBrush);
	}
	if (ArrowBrushes.Num() == 0)
	{
		return;
	}

	// 표시 크기의 2배로 그려서 DPI 확대나 축소 샘플링에서도 가장자리가 깨지지 않게 한다.
	float LargestSize = 0.f;
	for (const FSlateBrush* Brush : ArrowBrushes)
	{
		LargestSize = FMath::Max(LargestSize, static_cast<float>(FMath::Max(Brush->ImageSize.X, Brush->ImageSize.Y)));
	}
	const int32 TextureSize = FMath::Clamp(FMath::CeilToInt(LargestSize * 2.f), 16, 128);

	GeneratedArrowTexture = ShooterMapWidgetRules::CreateArrowTexture(TextureSize);
	if (GeneratedArrowTexture == nullptr)
	{
		return;
	}

	for (FSlateBrush* Brush : ArrowBrushes)
	{
		// ImageSize는 유지하고 이미지만 바꾼다.
		const FVector2D ImageSize = Brush->ImageSize;
		Brush->SetResourceObject(GeneratedArrowTexture);
		Brush->ImageSize = ImageSize;
	}
}

void UShooterMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 첫 변환이 적용되기 전 한 프레임 동안 원래 크기 맵이 보이는 걸 막는다.
	bHasValidView = false;
	if (Panel_MapContent)
	{
		Panel_MapContent->SetVisibility(ESlateVisibility::Hidden);
	}

	if (UShooterMinimapSubsystem* Subsystem = UShooterMinimapSubsystem::Get(this))
	{
		MapBoundsChangedHandle = Subsystem->OnMapBoundsChanged.AddUObject(this, &ThisClass::HandleMapBoundsChanged);
		HandleMapBoundsChanged(Subsystem->GetMapBounds());
	}

	if (AShooterPlayerController* PC = GetOwningPlayer<AShooterPlayerController>())
	{
		BoundPlayerController = PC;
		PC->OnPingReceived.AddUniqueDynamic(this, &ThisClass::HandlePingReceived);

		if (MapMode == EShooterMapMode::FullMap)
		{
			FullMapOpenChangedHandle = PC->OnFullMapOpenChanged.AddUObject(this, &ThisClass::HandleFullMapOpenChanged);
			FullMapZoomInputHandle = PC->OnFullMapZoomInput.AddUObject(this, &ThisClass::HandleFullMapZoomInput);
		}
	}

	if (MapMode == EShooterMapMode::FullMap)
	{
		// 전체 맵은 닫힌 채로 시작하고, 열릴 때만 갱신한다. 열림 상태의 주인은 컨트롤러다.
		bFullMapOpen = false;
		SetVisibility(ESlateVisibility::Collapsed);
		if (BoundPlayerController.IsValid() && BoundPlayerController->IsFullMapOpen())
		{
			SetFullMapOpen(true);
		}
	}
	else
	{
		RebuildPlayerMarkers();
		StartPositionUpdates();
	}
}

void UShooterMapWidget::NativeDestruct()
{
	StopPositionUpdates();
	ClearAllPings();
	UnbindAllPlayerStates();

	if (UShooterMinimapSubsystem* Subsystem = UShooterMinimapSubsystem::Get(this))
	{
		Subsystem->OnMapBoundsChanged.Remove(MapBoundsChangedHandle);
	}
	MapBoundsChangedHandle.Reset();
	BoundMapBounds.Reset();

	if (BoundPlayerController.IsValid())
	{
		BoundPlayerController->OnPingReceived.RemoveAll(this);
		if (FullMapOpenChangedHandle.IsValid())
		{
			BoundPlayerController->OnFullMapOpenChanged.Remove(FullMapOpenChangedHandle);
			// 위젯이 사라지면 보이지 않는 맵이 열림으로 남지 않게 컨트롤러 상태도 닫는다.
			BoundPlayerController->SetFullMapOpen(false);
		}
	}
	if (BoundPlayerController.IsValid())
	{
		BoundPlayerController->OnFullMapZoomInput.Remove(FullMapZoomInputHandle);
	}
	FullMapOpenChangedHandle.Reset();
	FullMapZoomInputHandle.Reset();
	BoundPlayerController.Reset();

	Super::NativeDestruct();
}

void UShooterMapWidget::ApplyLayoutToSlots()
{
	if (Panel_MapViewport)
	{
		// 미니맵에서 확대된 Content가 창 밖으로 삐져나오지 않게 한다.
		Panel_MapViewport->SetClipping(EWidgetClipping::ClipToBounds);
	}

	if (UCanvasPanelSlot* ContentSlot = Panel_MapContent ? Cast<UCanvasPanelSlot>(Panel_MapContent->Slot) : nullptr)
	{
		// 좌상단 원점 + 고정 크기. 이동/확대/회전은 전부 RenderTransform으로 한다.
		ContentSlot->SetAnchors(FAnchors(0.f, 0.f));
		ContentSlot->SetAlignment(FVector2D::ZeroVector);
		ContentSlot->SetPosition(FVector2D::ZeroVector);
		ContentSlot->SetSize(MapContentSize);
		ContentSlot->SetAutoSize(false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ShooterMapWidget] Panel_MapContent는 Panel_MapViewport(CanvasPanel)의 직계 자식이어야 합니다."));
	}

	if (UCanvasPanelSlot* BackgroundSlot = Image_MapBackground ? Cast<UCanvasPanelSlot>(Image_MapBackground->Slot) : nullptr)
	{
		BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackgroundSlot->SetOffsets(FMargin(0.f));
		BackgroundSlot->SetZOrder(0);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ShooterMapWidget] Image_MapBackground는 Panel_MapContent(CanvasPanel)의 직계 자식이어야 합니다."));
	}
}

void UShooterMapWidget::ApplyBackgroundTexture()
{
	const AShooterMinimapBounds* Bounds = BoundMapBounds.Get();
	UTexture2D* MapTexture = Bounds ? Bounds->GetMapTexture() : nullptr;

	// 레벨에 텍스처가 없으면 WBP에서 지정한 브러시를 그대로 둔다.
	if (Image_MapBackground && MapTexture)
	{
		Image_MapBackground->SetBrushFromTexture(MapTexture, false);
	}
}

// ---------------------------------------------------------------------------------------------
// 이벤트
// ---------------------------------------------------------------------------------------------

void UShooterMapWidget::HandleMapBoundsChanged(AShooterMinimapBounds* NewBounds)
{
	BoundMapBounds = NewBounds;
	ApplyBackgroundTexture();

	// 기준 액터가 없는 레벨(훈련장 등)은 같은 HUD를 써도 맵 UI를 띄우지 않는다.
	if (MapMode == EShooterMapMode::FullMap)
	{
		if (NewBounds == nullptr && BoundPlayerController.IsValid())
		{
			BoundPlayerController->SetFullMapOpen(false);
		}
	}
	else
	{
		SetVisibility(NewBounds ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 기준이 바뀌면 이전 기준으로 계산한 핑 좌표는 의미가 없다.
	ClearAllPings();
	RefreshMarkers();
}

void UShooterMapWidget::HandleFullMapOpenChanged(const bool bOpen)
{
	SetFullMapOpen(bOpen);
}

void UShooterMapWidget::HandleFullMapZoomInput(const float WheelDelta)
{
	if (!bFullMapOpen)
	{
		return;
	}

	FullMapTargetZoom = FMath::Clamp(
		FullMapTargetZoom + FMath::Sign(WheelDelta) * FullMapZoomStep, 1.f, FMath::Max(1.f, FullMapMaxZoom));

	if (FullMapZoomInterpSpeed <= 0.f)
	{
		FullMapCurrentZoom = FullMapTargetZoom;
	}

	// 다음 주기를 기다리지 않고 바로 반응을 보여준다.
	UpdatePositions();
}

void UShooterMapWidget::HandlePingReceived(const FPingEvent& Ping)
{
	const AShooterMinimapBounds* Bounds = BoundMapBounds.Get();
	if (Bounds == nullptr)
	{
		return;
	}

	const int32 PingIndex = GetOrCreateFreePingIndex();
	if (!PingMarkers.IsValidIndex(PingIndex))
	{
		return;
	}

	FShooterMapPingMarker& Marker = PingMarkers[PingIndex];
	Marker.bActive = true;
	Marker.ContentPosition = Bounds->WorldToMapUV(Ping.Location) * MapContentSize;
	Marker.Image->SetBrush(Ping.Type == EShooterPingType::Enemy ? PingEnemyBrush : PingLocationBrush);

	// 다음 위치 갱신을 기다리지 않고 지금 변환으로 바로 배치한다.
	Marker.Image->SetRenderTransform(FWidgetTransform(
		bClampPingsToEdge ? ClampToMinimapEdge(Marker.ContentPosition) : Marker.ContentPosition,
		FVector2D(1.f / CurrentContentScale),
		FVector2D::ZeroVector,
		-CurrentContentAngle));
	Marker.Image->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			Marker.ExpiryTimerHandle,
			FTimerDelegate::CreateUObject(this, &ThisClass::HandlePingExpired, PingIndex),
			ShooterPingLimits::PingLifetimeSeconds,
			false);
	}
}

void UShooterMapWidget::HandlePingExpired(const int32 PingIndex)
{
	if (!PingMarkers.IsValidIndex(PingIndex))
	{
		return;
	}

	FShooterMapPingMarker& Marker = PingMarkers[PingIndex];
	Marker.bActive = false;
	if (Marker.Image)
	{
		Marker.Image->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UShooterMapWidget::HandlePlayerTeamChanged(EGameTeam PreviousTeam, EGameTeam NewTeam)
{
	RefreshMarkers();
}

void UShooterMapWidget::HandlePlayerAliveChanged(bool bNewIsAlive)
{
	RefreshMarkers();
}

void UShooterMapWidget::HandlePlayerSpottedChanged(bool bNewIsSpotted)
{
	// 팀원이 적을 보거나 놓친 순간. 빨간 점 추가/삭제는 여기서만 일어난다.
	RefreshMarkers();
}

// ---------------------------------------------------------------------------------------------
// 상태
// ---------------------------------------------------------------------------------------------

void UShooterMapWidget::SetFullMapOpen(const bool bOpen)
{
	if (MapMode != EShooterMapMode::FullMap || bFullMapOpen == bOpen)
	{
		return;
	}

	bFullMapOpen = bOpen;

	if (bOpen)
	{
		// 닫기 전에 쓰던 배율 그대로 연다. 닫힐 때 보간 중이었더라도 목표 배율로 바로 맞춰 열리는 순간 줌 애니메이션이 없게 한다.
		FullMapCurrentZoom = FullMapTargetZoom;

		SetVisibility(ESlateVisibility::HitTestInvisible);
		RebuildPlayerMarkers();
		StartPositionUpdates();
		UpdatePositions();
	}
	else
	{
		StopPositionUpdates();
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UShooterMapWidget::StartPositionUpdates()
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetTimerManager().IsTimerActive(PositionUpdateTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PositionUpdateTimerHandle,
		this,
		&ThisClass::UpdatePositions,
		PositionUpdateInterval,
		true);
}

void UShooterMapWidget::StopPositionUpdates()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PositionUpdateTimerHandle);
	}
}

void UShooterMapWidget::RefreshMarkers()
{
	RebuildPlayerMarkers();

	// 닫힌 전체 맵은 열 때 어차피 다시 만든다.
	if (UWorld* World = GetWorld(); World && World->GetTimerManager().IsTimerActive(PositionUpdateTimerHandle))
	{
		UpdatePositions();
	}
}

void UShooterMapWidget::RebuildPlayerMarkers()
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState == nullptr || Panel_MapContent == nullptr)
	{
		return;
	}

	CachedPlayerArrayNum = GameState->PlayerArray.Num();

	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const EGameTeam LocalTeam = LocalPlayerState ? LocalPlayerState->GetTeam() : EGameTeam::None;

	int32 UsedCount = 0;
	for (APlayerState* RawPlayerState : GameState->PlayerArray)
	{
		ABaseTeamPlayerState* PlayerState = Cast<ABaseTeamPlayerState>(RawPlayerState);
		if (!IsValid(PlayerState))
		{
			continue;
		}

		// 지금 안 그려도 나중에 살아나거나 발견되면 이벤트로 알 수 있게 모두 구독해둔다.
		BindPlayerState(PlayerState);

		if (!PlayerState->IsAlive())
		{
			continue;
		}

		const EGameTeam Team = PlayerState->GetTeam();
		const bool bIsLocal = PlayerState == LocalPlayerState;
		const bool bHasTeams = LocalTeam != EGameTeam::None && Team != EGameTeam::None;
		const bool bIsTeammate = !bIsLocal && bHasTeams && Team == LocalTeam;
		const bool bIsSpottedEnemy = !bIsLocal && bHasTeams && Team != LocalTeam && PlayerState->IsSpottedByEnemy();

		const FSlateBrush* Brush = nullptr;
		int32 ZOrder = 0;
		if (bIsLocal)
		{
			Brush = &SelfMarkerBrush;
			ZOrder = ShooterMapWidgetRules::SelfZOrder;
		}
		else if (bIsTeammate)
		{
			Brush = &TeammateMarkerBrush;
			ZOrder = ShooterMapWidgetRules::TeammateZOrder;
		}
		else if (bIsSpottedEnemy)
		{
			Brush = &EnemyMarkerBrush;
			ZOrder = ShooterMapWidgetRules::EnemyZOrder;
		}

		if (Brush == nullptr)
		{
			continue;
		}

		FShooterMapPlayerMarker& Marker = GetOrCreatePlayerMarker(UsedCount++);
		Marker.Target = PlayerState;
		Marker.bIsLocalPlayer = bIsLocal;
		Marker.bShowsFacing = bIsLocal || (bIsTeammate && bShowTeammateFacing);
		Marker.bClampToEdge = bIsTeammate ? bClampTeammatesToEdge : (bIsSpottedEnemy && bClampEnemiesToEdge);
		Marker.Image->SetBrush(*Brush);

		if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker.Image->Slot))
		{
			MarkerSlot->SetZOrder(ZOrder);
		}
	}

	ActivePlayerMarkerCount = UsedCount;

	// 남는 마커는 숨긴다. 발견이 풀린 적의 빨간 점이 여기서 지워진다.
	for (int32 Index = UsedCount; Index < PlayerMarkers.Num(); ++Index)
	{
		PlayerMarkers[Index].Target.Reset();
		if (PlayerMarkers[Index].Image)
		{
			PlayerMarkers[Index].Image->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UShooterMapWidget::UpdatePositions()
{
	const AShooterMinimapBounds* Bounds = BoundMapBounds.Get();
	if (Bounds == nullptr || Panel_MapViewport == nullptr || Panel_MapContent == nullptr)
	{
		return;
	}

	// 합류/이탈은 이벤트가 없어 여기서 수만 비교한다.
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState && GameState->PlayerArray.Num() != CachedPlayerArrayNum)
	{
		RebuildPlayerMarkers();
	}

	// 첫 프레임은 지오메트리가 0이라 배치가 틀어지므로 건너뛴다.
	const FVector2D ViewportSize = Panel_MapViewport->GetCachedGeometry().GetLocalSize();
	if (ViewportSize.IsNearlyZero() || MapContentSize.IsNearlyZero())
	{
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	float ViewYaw = 0.f;
	const bool bHasViewPoint = GetViewPoint(ViewLocation, ViewYaw);

	// 1. Content(맵 전체) 변환
	FVector2D Pivot(0.5f, 0.5f);
	FVector2D Translation = (ViewportSize - MapContentSize) * 0.5f;
	float Scale = FMath::Min(ViewportSize.X / MapContentSize.X, ViewportSize.Y / MapContentSize.Y);
	float Angle = 0.f;
	CurrentEdgeRadius = 0.f;

	if (MapMode == EShooterMapMode::Minimap && bHasViewPoint)
	{
		// 보는 지점을 피벗으로 확대/회전한 뒤 창 중앙으로 끌어온다.
		const FVector2D CenterUV = Bounds->WorldToMapUV(ViewLocation);
		const float ViewRadiusInContent =
			MinimapViewRadius / (Bounds->GetMapWorldHalfSize() * 2.f) * MapContentSize.X;

		Pivot = CenterUV;
		Translation = ViewportSize * 0.5f - CenterUV * MapContentSize;
		Scale = (FMath::Min(ViewportSize.X, ViewportSize.Y) * 0.5f) / FMath::Max(ViewRadiusInContent, 1.f);
		Angle = bRotateWithView ? -Bounds->WorldYawToMapAngle(ViewYaw) : 0.f;

		// 원형 마스크 반지름(화면 px)을 Content 단위로 환산. 회전은 길이를 바꾸지 않으므로 확대만 되돌리면 된다.
		CurrentViewCenter = CenterUV * MapContentSize;
		CurrentEdgeRadius = FMath::Max(0.f, FMath::Min(ViewportSize.X, ViewportSize.Y) * 0.5f - EdgeMarkerInset) / Scale;
	}
	else if (MapMode == EShooterMapMode::FullMap)
	{
		// 휠로 바뀐 목표 배율을 위치 갱신 주기마다 부드럽게 따라간다.
		if (FullMapZoomInterpSpeed > 0.f && !FMath::IsNearlyEqual(FullMapCurrentZoom, FullMapTargetZoom, 0.001f))
		{
			FullMapCurrentZoom = FMath::FInterpTo(FullMapCurrentZoom, FullMapTargetZoom, PositionUpdateInterval, FullMapZoomInterpSpeed);
		}
		else
		{
			FullMapCurrentZoom = FullMapTargetZoom;
		}

		if (FullMapCurrentZoom > 1.001f)
		{
			// 1배(맵 전체 맞춤) 기준으로 확대하고, 내 위치를 중심으로 둔다.
			Scale *= FullMapCurrentZoom;

			FVector2D Center = MapContentSize * 0.5f;
			if (bHasViewPoint)
			{
				Center = Bounds->WorldToMapUV(ViewLocation) * MapContentSize;
			}

			// 맵 가장자리 바깥(빈 공간)이 창에 들어오지 않게 중심을 안쪽으로 붙잡는다.
			const FVector2D HalfVisible = ViewportSize * 0.5f / Scale;
			Center.X = HalfVisible.X * 2.f >= MapContentSize.X
				? MapContentSize.X * 0.5f
				: FMath::Clamp(Center.X, HalfVisible.X, MapContentSize.X - HalfVisible.X);
			Center.Y = HalfVisible.Y * 2.f >= MapContentSize.Y
				? MapContentSize.Y * 0.5f
				: FMath::Clamp(Center.Y, HalfVisible.Y, MapContentSize.Y - HalfVisible.Y);

			Pivot = Center / MapContentSize;
			Translation = ViewportSize * 0.5f - Center;
		}
	}

	Panel_MapContent->SetRenderTransformPivot(Pivot);
	Panel_MapContent->SetRenderTransform(FWidgetTransform(Translation, FVector2D(Scale), FVector2D::ZeroVector, Angle));

	CurrentContentScale = Scale;
	CurrentContentAngle = Angle;

	// 마커는 Content 확대를 되돌려서 화면상 크기를 일정하게 유지한다.
	const FVector2D MarkerScale(1.f / Scale);

	// 2. 플레이어 마커
	const APlayerController* PC = GetOwningPlayer();
	for (int32 Index = 0; Index < ActivePlayerMarkerCount; ++Index)
	{
		FShooterMapPlayerMarker& Marker = PlayerMarkers[Index];
		const ABaseTeamPlayerState* PlayerState = Marker.Target.Get();
		const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!IsValid(Pawn))
		{
			Marker.Image->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FVector2D WorldContentPosition = Bounds->WorldToMapUV(Pawn->GetActorLocation()) * MapContentSize;
		const FVector2D ContentPosition = Marker.bClampToEdge ? ClampToMinimapEdge(WorldContentPosition) : WorldContentPosition;

		float MarkerAngle = -Angle;
		if (Marker.bShowsFacing)
		{
			// 내 화살표는 마우스 회전이 바로 보이도록 ControlRotation을 쓴다.
			const float FacingYaw = (Marker.bIsLocalPlayer && PC)
				? PC->GetControlRotation().Yaw
				: Pawn->GetActorRotation().Yaw;
			MarkerAngle = Bounds->WorldYawToMapAngle(FacingYaw);
		}

		Marker.Image->SetRenderTransform(FWidgetTransform(ContentPosition, MarkerScale, FVector2D::ZeroVector, MarkerAngle));
		if (Marker.Image->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			Marker.Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// 3. 핑 마커: 월드 위치는 고정이지만, 내가 움직이면 테두리 고정/실제 위치 전환이 바뀌므로 매번 다시 계산한다.
	for (FShooterMapPingMarker& Marker : PingMarkers)
	{
		if (Marker.bActive && Marker.Image)
		{
			const FVector2D ContentPosition = bClampPingsToEdge ? ClampToMinimapEdge(Marker.ContentPosition) : Marker.ContentPosition;
			Marker.Image->SetRenderTransform(
				FWidgetTransform(ContentPosition, MarkerScale, FVector2D::ZeroVector, -Angle));
		}
	}

	if (!bHasValidView)
	{
		bHasValidView = true;
		Panel_MapContent->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

FVector2D UShooterMapWidget::ClampToMinimapEdge(const FVector2D& ContentPosition) const
{
	if (CurrentEdgeRadius <= 0.f)
	{
		return ContentPosition;
	}

	// 중심 → 대상 방향은 유지하고 거리만 테두리 반지름으로 자른다.
	const FVector2D Offset = ContentPosition - CurrentViewCenter;
	const double Distance = Offset.Size();
	if (Distance <= CurrentEdgeRadius)
	{
		return ContentPosition;
	}

	return CurrentViewCenter + Offset * (CurrentEdgeRadius / Distance);
}

bool UShooterMapWidget::GetViewPoint(FVector& OutLocation, float& OutYaw) const
{
	const APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return false;
	}

	if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
	{
		OutLocation = CameraManager->GetCameraLocation();
		OutYaw = CameraManager->GetCameraRotation().Yaw;
		return true;
	}

	if (const APawn* Pawn = PC->GetPawn())
	{
		OutLocation = Pawn->GetActorLocation();
		OutYaw = PC->GetControlRotation().Yaw;
		return true;
	}

	return false;
}

// ---------------------------------------------------------------------------------------------
// 구독 / 풀
// ---------------------------------------------------------------------------------------------

void UShooterMapWidget::BindPlayerState(ABaseTeamPlayerState* PlayerState)
{
	if (BoundPlayerStates.Contains(PlayerState))
	{
		return;
	}

	PlayerState->OnPlayerTeamChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerTeamChanged);
	PlayerState->OnPlayerAliveStateChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerAliveChanged);
	PlayerState->OnSpottedByEnemyChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerSpottedChanged);

	// 떠난 플레이어의 약참조는 여기서 같이 정리한다.
	BoundPlayerStates.RemoveAll([](const TWeakObjectPtr<ABaseTeamPlayerState>& Bound) { return !Bound.IsValid(); });
	BoundPlayerStates.Add(PlayerState);
}

void UShooterMapWidget::UnbindAllPlayerStates()
{
	for (const TWeakObjectPtr<ABaseTeamPlayerState>& Bound : BoundPlayerStates)
	{
		if (ABaseTeamPlayerState* PlayerState = Bound.Get())
		{
			PlayerState->OnPlayerTeamChanged.RemoveAll(this);
			PlayerState->OnPlayerAliveStateChanged.RemoveAll(this);
			PlayerState->OnSpottedByEnemyChanged.RemoveAll(this);
		}
	}
	BoundPlayerStates.Reset();
	CachedPlayerArrayNum = INDEX_NONE;
}

void UShooterMapWidget::ClearAllPings()
{
	UWorld* World = GetWorld();
	for (FShooterMapPingMarker& Marker : PingMarkers)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(Marker.ExpiryTimerHandle);
		}

		Marker.bActive = false;
		if (Marker.Image)
		{
			Marker.Image->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

FShooterMapPlayerMarker& UShooterMapWidget::GetOrCreatePlayerMarker(const int32 Index)
{
	while (!PlayerMarkers.IsValidIndex(Index))
	{
		FShooterMapPlayerMarker& NewMarker = PlayerMarkers.AddDefaulted_GetRef();
		NewMarker.Image = CreateMarkerImage(ShooterMapWidgetRules::TeammateZOrder);
	}

	return PlayerMarkers[Index];
}

int32 UShooterMapWidget::GetOrCreateFreePingIndex()
{
	for (int32 Index = 0; Index < PingMarkers.Num(); ++Index)
	{
		if (!PingMarkers[Index].bActive && PingMarkers[Index].Image)
		{
			return Index;
		}
	}

	UImage* Image = CreateMarkerImage(ShooterMapWidgetRules::PingZOrder);
	if (Image == nullptr)
	{
		return INDEX_NONE;
	}

	FShooterMapPingMarker& NewMarker = PingMarkers.AddDefaulted_GetRef();
	NewMarker.Image = Image;
	return PingMarkers.Num() - 1;
}

UImage* UShooterMapWidget::CreateMarkerImage(const int32 ZOrder)
{
	if (WidgetTree == nullptr || Panel_MapContent == nullptr)
	{
		return nullptr;
	}

	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Image->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* MarkerSlot = Panel_MapContent->AddChildToCanvas(Image))
	{
		// 슬롯은 원점에 두고 RenderTranslation으로만 옮긴다. 레이아웃 재계산 없이 이동만 다시 그린다.
		MarkerSlot->SetAnchors(FAnchors(0.f, 0.f));
		MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MarkerSlot->SetPosition(FVector2D::ZeroVector);
		MarkerSlot->SetAutoSize(true);
		MarkerSlot->SetZOrder(ZOrder);
	}

	return Image;
}
