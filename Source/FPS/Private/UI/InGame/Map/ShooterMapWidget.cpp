// ShooterMapWidget.cpp
#include "UI/InGame/Map/ShooterMapWidget.h"

#include "Actor/DominationCapturePoint.h"
#include "Actor/DominationPointGimmick.h"
#include "Actor/DominationZoneWall.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameMode/State/DominationGameState.h"
#include "GameFramework/Pawn.h"
#include "Minimap/ShooterMinimapBounds.h"
#include "Minimap/ShooterMinimapSubsystem.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "TextureResource.h"

namespace ShooterMapWidgetRules
{
	// 겹칠 때 위에 그려질 순서. 배경은 0.
	constexpr int32 ZoneMaskZOrder = 1;
	constexpr int32 PointHighlightZOrder = 2;
	constexpr int32 ZoneWallZOrder = 3;
	constexpr int32 GimmickStatusZOrder = 4;

	// 확보 완료 직후 깜빡임 한 번의 길이(초). 평소 깜빡임보다 빠르게 해 눈에 띄게 한다.
	constexpr float GimmickFlashPeriod = 0.25f;
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

	if (MapMode == EShooterMapMode::FullMap)
	{
		// 창 모양을 구역 비율에 맞추려면 Panel_MapViewport를 감싼 SizeBox가 필요하다. WBP에 둔 크기가 기본 최대 크기다.
		for (UPanelWidget* Parent = Panel_MapViewport ? Panel_MapViewport->GetParent() : nullptr; Parent; Parent = Parent->GetParent())
		{
			if (USizeBox* SizeBox = Cast<USizeBox>(Parent))
			{
				FullMapSizeBox = SizeBox;
				FullMapBaseSize = FVector2D(
					SizeBox->IsWidthOverride() ? SizeBox->GetWidthOverride() : 0.f,
					SizeBox->IsHeightOverride() ? SizeBox->GetHeightOverride() : 0.f);
				break;
			}
		}

		if (FullMapSizeBox == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShooterMapWidget] 전체 맵 창 모양을 구역에 맞추려면 Panel_MapViewport를 SizeBox로 감싸야 합니다."));
		}
	}
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
		HandleMapBoundsChanged(Subsystem->ResolveMapBounds());
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
	bZoneDirty = true;

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
	if (!BoundMapBounds.IsValid())
	{
		// 기준을 잃은 채로 남지 않게(레이트 조인/킬캠 이후 등) 서브시스템에 다시 찾아달라고 한다.
		// 새로 등록되면 OnMapBoundsChanged로 이미 들어왔을 수 있으니 그때는 다시 적용하지 않는다.
		UShooterMinimapSubsystem* Subsystem = UShooterMinimapSubsystem::Get(this);
		AShooterMinimapBounds* ResolvedBounds = Subsystem ? Subsystem->ResolveMapBounds() : nullptr;
		if (ResolvedBounds && !BoundMapBounds.IsValid())
		{
			HandleMapBoundsChanged(ResolvedBounds);
		}
		return;
	}

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
	// 창 크기 기준을 아직 못 잡았으면(첫 배치 전) 잡힐 때까지 다시 시도한다.
	if (FullMapSizeBox && FullMapViewportSize.IsNearlyZero())
	{
		ApplyFullMapViewportShape();
	}

	// 전체 맵은 창 크기를 코드가 정하므로, 레이아웃이 반영되기 전 프레임에도 정한 크기로 계산한다.
	const FVector2D ViewportSize = !FullMapViewportSize.IsNearlyZero()
		? FullMapViewportSize
		: Panel_MapViewport->GetCachedGeometry().GetLocalSize();
	if (ViewportSize.IsNearlyZero() || MapContentSize.IsNearlyZero())
	{
		return;
	}

	RefreshActiveZone(*Bounds);

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

		// 1배는 보여줄 영역(활성 구역, 없으면 빈 공간을 뺀 맵 전체)을 창에 맞춘다.
		const FVector2D FocusMin = bHasActiveZone ? ActiveZoneMin : PlayableAreaMin;
		const FVector2D FocusMax = bHasActiveZone ? ActiveZoneMax : PlayableAreaMax;
		const FVector2D FocusSize = FocusMax - FocusMin;
		Scale = FMath::Min(ViewportSize.X / FocusSize.X, ViewportSize.Y / FocusSize.Y);

		FVector2D Center = (FocusMin + FocusMax) * 0.5f;
		if (FullMapCurrentZoom > 1.001f)
		{
			// 1배 기준으로 확대하고, 내 위치를 중심으로 둔다.
			Scale *= FullMapCurrentZoom;
			if (bHasViewPoint)
			{
				Center = Bounds->WorldToMapUV(ViewLocation) * MapContentSize;
			}

			// 보여줄 영역 바깥이 창에 들어오지 않게 중심을 안쪽으로 붙잡는다.
			const FVector2D HalfVisible = ViewportSize * 0.5f / Scale;
			Center.X = HalfVisible.X * 2.f >= FocusSize.X
				? (FocusMin.X + FocusMax.X) * 0.5f
				: FMath::Clamp(Center.X, FocusMin.X + HalfVisible.X, FocusMax.X - HalfVisible.X);
			Center.Y = HalfVisible.Y * 2.f >= FocusSize.Y
				? (FocusMin.Y + FocusMax.Y) * 0.5f
				: FMath::Clamp(Center.Y, FocusMin.Y + HalfVisible.Y, FocusMax.Y - HalfVisible.Y);
		}

		Pivot = Center / MapContentSize;
		Translation = ViewportSize * 0.5f - Center;
	}

	Panel_MapContent->SetRenderTransformPivot(Pivot);
	Panel_MapContent->SetRenderTransform(FWidgetTransform(Translation, FVector2D(Scale), FVector2D::ZeroVector, Angle));

	CurrentContentScale = Scale;
	CurrentContentAngle = Angle;

	UpdateDominationOverlays(Scale);
	UpdateGimmickStatus(*Bounds);

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

void UShooterMapWidget::RefreshActiveZone(const AShooterMinimapBounds& Bounds)
{
	UWorld* World = GetWorld();
	const ADominationGameState* GameState = World ? World->GetGameState<ADominationGameState>() : nullptr;
	const EDominationPointId PointId = GameState ? GameState->GetActivePointId() : EDominationPointId::None;

	// 텍스처 위아래(또는 좌우)의 빈 공간을 뺀 실제 맵 영역. 구역이 없을 때 보여줄 영역이자 구역의 바깥 한계다.
	FVector2D PlayableMinUV = Bounds.GetPlayableAreaUVMin().ClampAxes(0.0, 1.0);
	FVector2D PlayableMaxUV = Bounds.GetPlayableAreaUVMax().ClampAxes(0.0, 1.0);
	if (PlayableMaxUV.X <= PlayableMinUV.X || PlayableMaxUV.Y <= PlayableMinUV.Y)
	{
		PlayableMinUV = FVector2D::ZeroVector;
		PlayableMaxUV = FVector2D::UnitVector;
	}
	PlayableAreaMin = PlayableMinUV * MapContentSize;
	PlayableAreaMax = PlayableMaxUV * MapContentSize;

	if (!bZoneDirty && PointId == CachedZonePointId)
	{
		return;
	}

	// 클라이언트는 GameState 복제가 늦을 수 있어, 잡힐 때까지는 매 갱신마다 다시 본다.
	bZoneDirty = GameState == nullptr;
	CachedZonePointId = PointId;
	bHasActiveZone = false;

	// 좌상단 미니맵은 내 주변만 보여주므로 구역 제한은 전체 맵(M)에만 건다.
	if (MapMode == EShooterMapMode::FullMap && bLimitToActiveZone && GameState)
	{
		// 벽마다 안쪽(Forward)이 맵의 어느 변을 향하는지 보고, 맵 전체 사각형에서 그 반대쪽을 잘라낸다.
		// 맵 끝이나 건물로 막힌 쪽은 벽이 없어도 맵 가장자리가 경계가 되므로 벽이 하나뿐인 거점도 잡힌다.
		// 같은 변에 벽이 여럿이면(통로 여러 개) 가장 바깥 벽을 경계로 써서 구역이 잘려 나가지 않게 한다.
		const float PaddingUV = ActiveZonePadding / (Bounds.GetMapWorldHalfSize() * 2.f);
		TOptional<double> SideLimits[4]; // MinX, MaxX, MinY, MaxY (UV)
		for (TActorIterator<ADominationZoneWall> It(World); It; ++It)
		{
			const ADominationZoneWall* Wall = *It;
			if (!IsValid(Wall) || UShooterMinimapSubsystem::IsKillcamDuplicate(Wall) || !Wall->IsActiveForPoint(PointId))
			{
				continue;
			}

			const FVector2D WallUV = Bounds.WorldToMapUV(Wall->GetActorLocation());
			const FVector2D InsideDir = Bounds.WorldToMapUV(Wall->GetActorLocation() + Wall->GetActorForwardVector() * 100.f) - WallUV;
			const bool bAlongX = FMath::Abs(InsideDir.X) >= FMath::Abs(InsideDir.Y);
			const bool bPositive = bAlongX ? InsideDir.X > 0.0 : InsideDir.Y > 0.0;
			const double Coordinate = bAlongX ? WallUV.X : WallUV.Y;

			// 안쪽이 +방향이면 그 벽이 최소 경계, -방향이면 최대 경계.
			const int32 Side = (bAlongX ? 0 : 2) + (bPositive ? 0 : 1);
			const double Limit = bPositive ? Coordinate - PaddingUV : Coordinate + PaddingUV;
			SideLimits[Side] = SideLimits[Side].IsSet()
				? (bPositive ? FMath::Min(SideLimits[Side].GetValue(), Limit) : FMath::Max(SideLimits[Side].GetValue(), Limit))
				: Limit;
		}

		if (SideLimits[0] || SideLimits[1] || SideLimits[2] || SideLimits[3])
		{
			const FVector2D ZoneMinUV(
				FMath::Max(PlayableMinUV.X, SideLimits[0].Get(PlayableMinUV.X)),
				FMath::Max(PlayableMinUV.Y, SideLimits[2].Get(PlayableMinUV.Y)));
			const FVector2D ZoneMaxUV(
				FMath::Min(PlayableMaxUV.X, SideLimits[1].Get(PlayableMaxUV.X)),
				FMath::Min(PlayableMaxUV.Y, SideLimits[3].Get(PlayableMaxUV.Y)));
			ActiveZoneMin = ZoneMinUV * MapContentSize;
			ActiveZoneMax = ZoneMaxUV * MapContentSize;
			bHasActiveZone = ActiveZoneMax.X - ActiveZoneMin.X > 1.f && ActiveZoneMax.Y - ActiveZoneMin.Y > 1.f;
		}
	}

	ApplyZoneMask();
	ApplyFullMapViewportShape();
	RebuildDominationOverlays(Bounds, GameState, PointId);
}

void UShooterMapWidget::RebuildDominationOverlays(
	const AShooterMinimapBounds& Bounds,
	const ADominationGameState* GameState,
	const EDominationPointId PointId)
{
	ActiveZoneWallLineCount = 0;
	bHasPointHighlight = false;

	UWorld* World = GetWorld();
	const auto ToContent = [&Bounds, this](const FVector& WorldLocation)
	{
		return Bounds.WorldToMapUV(WorldLocation) * MapContentSize;
	};

	// 점령전이 아닌 레벨은 벽도 거점도 그리지 않는다.
	if (World && GameState)
	{
		// 1. 켜진 벽마다 벽 면(로컬 Y) 양 끝을 맵 좌표로 옮겨 선 하나로 잇는다.
		if (bShowZoneWalls)
		{
			for (TActorIterator<ADominationZoneWall> It(World); It; ++It)
			{
				const ADominationZoneWall* Wall = *It;
				if (!IsValid(Wall) || UShooterMinimapSubsystem::IsKillcamDuplicate(Wall) || !Wall->IsActiveForPoint(PointId))
				{
					continue;
				}

				const FTransform& WallTransform = Wall->GetActorTransform();
				const float HalfWidth = Wall->GetWallHalfExtent().Y;
				const FVector2D Start = ToContent(WallTransform.TransformPosition(FVector(0.f, -HalfWidth, 0.f)));
				const FVector2D End = ToContent(WallTransform.TransformPosition(FVector(0.f, HalfWidth, 0.f)));
				const FVector2D Delta = End - Start;
				if (Delta.IsNearlyZero())
				{
					continue;
				}

				if (ZoneWallLines.Num() <= ActiveZoneWallLineCount)
				{
					FShooterMapZoneWallLine& NewLine = ZoneWallLines.AddDefaulted_GetRef();
					NewLine.Image = CreateMarkerImage(ShooterMapWidgetRules::ZoneWallZOrder);
				}

				FShooterMapZoneWallLine& Line = ZoneWallLines[ActiveZoneWallLineCount];
				if (Line.Image == nullptr)
				{
					continue;
				}
				++ActiveZoneWallLineCount;

				Line.ContentCenter = (Start + End) * 0.5f;
				Line.ContentLength = Delta.Size();
				Line.Angle = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));

				// 길이는 Content 좌표로 고정하고, 두께는 높이 1짜리를 RenderTransform 배율로 늘린다(레이아웃 재계산 없음).
				if (UCanvasPanelSlot* LineSlot = Cast<UCanvasPanelSlot>(Line.Image->Slot))
				{
					LineSlot->SetAutoSize(false);
					LineSlot->SetPosition(Line.ContentCenter);
					LineSlot->SetSize(FVector2D(Line.ContentLength, 1.f));
				}
				Line.Image->SetColorAndOpacity(ZoneWallColor);
				Line.Image->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}

		// 2. 현재 거점(매치 시작 전엔 첫 거점) 박스를 맵 위에 같은 모양으로 덮는다.
		const EDominationPointId HighlightPointId = PointId == EDominationPointId::None ? DominationRules::InitialPointId : PointId;
		for (TActorIterator<ADominationCapturePoint> It(World); bHighlightActivePoint && It; ++It)
		{
			const ADominationCapturePoint* Point = *It;
			if (!IsValid(Point) || UShooterMinimapSubsystem::IsKillcamDuplicate(Point) || Point->GetPointId() != HighlightPointId)
			{
				continue;
			}

			if (PointHighlightImage == nullptr)
			{
				PointHighlightImage = CreateMarkerImage(ShooterMapWidgetRules::PointHighlightZOrder);
			}
			UCanvasPanelSlot* HighlightSlot = PointHighlightImage ? Cast<UCanvasPanelSlot>(PointHighlightImage->Slot) : nullptr;
			if (HighlightSlot == nullptr)
			{
				break;
			}

			// 박스 로컬 X(Forward)/Y(Right) 축이 맵에서 향하는 방향과 길이를 구한다. 맵의 가로가 Right 쪽이다.
			const FTransform& PointTransform = Point->GetActorTransform();
			const FVector Extent = Point->GetCaptureBoxExtent();
			const FVector2D Center = ToContent(PointTransform.GetLocation());
			const FVector2D RightAxis = ToContent(PointTransform.TransformPosition(FVector(0.f, Extent.Y, 0.f))) - Center;
			const FVector2D ForwardAxis = ToContent(PointTransform.TransformPosition(FVector(Extent.X, 0.f, 0.f))) - Center;

			HighlightSlot->SetAutoSize(false);
			HighlightSlot->SetPosition(Center);
			HighlightSlot->SetSize(FVector2D(RightAxis.Size(), ForwardAxis.Size()) * 2.f);
			PointHighlightImage->SetRenderTransform(
				FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D::ZeroVector,
					FMath::RadiansToDegrees(FMath::Atan2(RightAxis.Y, RightAxis.X))));
			bHasPointHighlight = true;
			break;
		}
	}

	for (int32 Index = ActiveZoneWallLineCount; Index < ZoneWallLines.Num(); ++Index)
	{
		if (ZoneWallLines[Index].Image)
		{
			ZoneWallLines[Index].Image->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (PointHighlightImage)
	{
		PointHighlightImage->SetVisibility(bHasPointHighlight ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UShooterMapWidget::UpdateDominationOverlays(const float ContentScale)
{
	// 벽 선은 Content 확대를 되돌려 화면상 두께를 일정하게 둔다.
	const float Thickness = ZoneWallThickness / FMath::Max(ContentScale, KINDA_SMALL_NUMBER);
	for (int32 Index = 0; Index < ActiveZoneWallLineCount; ++Index)
	{
		const FShooterMapZoneWallLine& Line = ZoneWallLines[Index];
		if (Line.Image)
		{
			Line.Image->SetRenderTransform(
				FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, Thickness), FVector2D::ZeroVector, Line.Angle));
		}
	}

	if (!bHasPointHighlight || PointHighlightImage == nullptr)
	{
		return;
	}

	// 다음 거점으로 넘어가는 중이거나 매치 시작 전이면 아직 점령할 수 없는 거점이라 깜빡인다.
	const UWorld* World = GetWorld();
	const ADominationGameState* GameState = World ? World->GetGameState<ADominationGameState>() : nullptr;
	const bool bUpcoming = GameState && (!GameState->HasActivePoint() || GameState->IsPointTransitioning());

	// 점령 가능한 거점은 점령 중인 팀 색(아군/적), 경합이면 경합 색으로 깜빡인다. 아무도 없으면 ActivePointColor.
	FLinearColor Color = bUpcoming ? UpcomingPointColor : ActivePointColor;
	bool bBlink = bUpcoming || bBlinkActivePoint;
	if (!bUpcoming && GameState && bColorPointByCapture)
	{
		// CapturingTeam은 게이지 주인이라, 상대 게이지를 깎는 동안엔 거점 안에 있는 팀과 다르다.
		// 그래서 색은 지금 거점 안에 있는 팀 기준으로 정하고, 게이지 주인과 다르면(되찾는 중) 깜빡여 구분한다.
		const FDominationCaptureState CaptureState = GameState->GetCaptureState();
		const EGameTeam OccupyingTeam = CaptureState.TeamAPlayerCount > 0 && CaptureState.TeamBPlayerCount == 0
			? EGameTeam::TeamA
			: (CaptureState.TeamBPlayerCount > 0 && CaptureState.TeamAPlayerCount == 0 ? EGameTeam::TeamB : EGameTeam::None);
		const bool bHasGauge = CaptureState.CapturingTeam != EGameTeam::None && CaptureState.CaptureProgress > 0.f;

		const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
		const EGameTeam LocalTeam = LocalPlayerState ? LocalPlayerState->GetTeam() : EGameTeam::None;
		const auto TeamPointColor = [this, LocalTeam](const EGameTeam Team, FLinearColor Fallback)
		{
			// 색은 팀 색, 진하기는 거점 강조 기준으로 맞춘다.
			switch (DominationRules::GetCaptureRelation(Team, LocalTeam))
			{
			case EDominationCaptureRelation::Ally:
				return AllyStatusColor.CopyWithNewOpacity(ActivePointColor.A);
			case EDominationCaptureRelation::Enemy:
				return EnemyStatusColor.CopyWithNewOpacity(ActivePointColor.A);
			default:
				return Fallback;
			}
		};

		if (CaptureState.bContested)
		{
			Color = ContestedPointColor;
			bBlink = true;
		}
		else if (OccupyingTeam != EGameTeam::None)
		{
			// 내 게이지를 올리는 중이면 고정, 상대 게이지를 깎는 중이면 깜빡임.
			Color = TeamPointColor(OccupyingTeam, Color);
			bBlink = bHasGauge && CaptureState.CapturingTeam != OccupyingTeam;
		}
		else if (bHasGauge)
		{
			// 아무도 없이 게이지만 남아 줄어드는 중. 주인 팀 색을 옅게 둔다.
			Color = TeamPointColor(CaptureState.CapturingTeam, Color);
			Color.A *= UnattendedPointAlphaScale;
		}
	}

	if (World && bBlink)
	{
		const float Phase = FMath::Fmod(World->GetTimeSeconds(), PointBlinkPeriod) / PointBlinkPeriod;
		Color.A *= 0.5f - 0.5f * FMath::Cos(Phase * UE_TWO_PI);
	}
	PointHighlightImage->SetColorAndOpacity(Color);
}

void UShooterMapWidget::UpdateGimmickStatus(const AShooterMinimapBounds& Bounds)
{
	const UWorld* World = GetWorld();
	if (!bShowGimmickStatus || World == nullptr)
	{
		return;
	}

	// 확보한 팀: 비콘은 소유 팀, 정찰 단말은 정찰 효과가 걸려 있는 동안의 팀.
	const auto GetHolderTeam = [](const ADominationPointGimmick& InGimmick)
	{
		if (InGimmick.GetGimmickType() == EDominationGimmickType::RespawnRelay)
		{
			return InGimmick.GetOwningTeam();
		}
		return InGimmick.GetReconTimeRemaining() > 0.f ? InGimmick.GetReconTeam() : EGameTeam::None;
	};

	// 켜진 기믹은 활성 거점마다 하나뿐이고, 거점이 바뀌어 꺼지면 다시 찾는다.
	ADominationPointGimmick* Gimmick = TrackedGimmick.Get();
	if (!IsValid(Gimmick) || !Gimmick->IsGimmickActive() || UShooterMinimapSubsystem::IsKillcamDuplicate(Gimmick))
	{
		Gimmick = nullptr;
		for (TActorIterator<ADominationPointGimmick> It(World); It; ++It)
		{
			if (IsValid(*It) && It->IsGimmickActive() && !UShooterMinimapSubsystem::IsKillcamDuplicate(*It))
			{
				Gimmick = *It;
				break;
			}
		}

		if (Gimmick != TrackedGimmick.Get())
		{
			// 새 기믹은 지금 상태를 기준으로 삼아, 맵을 연 순간 이미 확보된 기믹이 깜빡이지 않게 한다.
			TrackedGimmick = Gimmick;
			LastGimmickHolderTeam = Gimmick ? GetHolderTeam(*Gimmick) : EGameTeam::None;
			GimmickFlashEndTime = 0.f;
		}
	}

	if (Gimmick == nullptr)
	{
		if (GimmickStatusImage)
		{
			GimmickStatusImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (GimmickStatusImage == nullptr)
	{
		GimmickStatusImage = CreateMarkerImage(ShooterMapWidgetRules::GimmickStatusZOrder);
		if (GimmickStatusImage == nullptr)
		{
			return;
		}
		// 텍스처를 안 넣었으면 기믹 아이콘(원)에 맞춰 옅게 채운 원 + 테두리로 그린다.
		// 테두리 색은 이미지 틴트(ColorAndOpacity)의 색을 받지 않고 투명도만 받으므로, 색이 바뀔 때 브러시를 직접 바꾼다.
		GimmickStatusDrawBrush = GimmickStatusBrush;
		bGimmickStatusUsesCircle = GimmickStatusDrawBrush.GetResourceObject() == nullptr;
		if (bGimmickStatusUsesCircle)
		{
			GimmickStatusDrawBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			GimmickStatusDrawBrush.TintColor = FLinearColor(1.f, 1.f, 1.f, GimmickStatusFillOpacity);
			GimmickStatusDrawBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
			GimmickStatusDrawBrush.OutlineSettings.bUseBrushTransparency = true;
			GimmickStatusDrawBrush.OutlineSettings.Width = GimmickStatusOutlineWidth;
		}
		GimmickStatusImage->SetBrush(GimmickStatusDrawBrush);
		LastGimmickOutlineColor = FLinearColor::Transparent;
	}

	// 배경 텍스처의 아이콘과 같이 커지도록 월드 크기를 Content 좌표로 깐다.
	if (UCanvasPanelSlot* StatusSlot = Cast<UCanvasPanelSlot>(GimmickStatusImage->Slot))
	{
		const float ContentSize = GimmickStatusWorldSize / (Bounds.GetMapWorldHalfSize() * 2.f) * MapContentSize.X;
		StatusSlot->SetAutoSize(false);
		StatusSlot->SetPosition(Bounds.WorldToMapUV(Gimmick->GetActorLocation()) * MapContentSize);
		StatusSlot->SetSize(FVector2D(ContentSize));
	}

	const EGameTeam HolderTeam = GetHolderTeam(*Gimmick);
	const float Now = World->GetTimeSeconds();
	if (HolderTeam != LastGimmickHolderTeam)
	{
		LastGimmickHolderTeam = HolderTeam;
		GimmickFlashEndTime = HolderTeam != EGameTeam::None ? Now + GimmickSecuredFlashDuration : 0.f;
	}

	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const EGameTeam LocalTeam = LocalPlayerState ? LocalPlayerState->GetTeam() : EGameTeam::None;
	const auto RelationColor = [this, LocalTeam](const EGameTeam Team)
	{
		switch (DominationRules::GetCaptureRelation(Team, LocalTeam))
		{
		case EDominationCaptureRelation::Ally:
			return AllyStatusColor;
		case EDominationCaptureRelation::Enemy:
			return EnemyStatusColor;
		default:
			return NeutralStatusColor;
		}
	};

	// 누가 확보 중이면 그 팀 색으로 깜빡이고, 아니면 확보 팀 색. 확보 직후엔 잠깐 빠르게 깜빡인다.
	const EGameTeam InteractingTeam = Gimmick->GetInteractingTeam();
	FLinearColor Color = RelationColor(InteractingTeam != EGameTeam::None ? InteractingTeam : HolderTeam);
	const float BlinkPeriod = Now < GimmickFlashEndTime
		? ShooterMapWidgetRules::GimmickFlashPeriod
		: (InteractingTeam != EGameTeam::None ? PointBlinkPeriod : 0.f);
	if (BlinkPeriod > 0.f)
	{
		const float Phase = FMath::Fmod(Now, BlinkPeriod) / BlinkPeriod;
		Color.A *= 0.5f - 0.5f * FMath::Cos(Phase * UE_TWO_PI);
	}

	// 원 테두리에 팀 색을 입힌다. 같은 색이면 브러시를 다시 넣지 않는다.
	const FLinearColor OutlineColor = Color.CopyWithNewOpacity(1.f);
	if (bGimmickStatusUsesCircle && !OutlineColor.Equals(LastGimmickOutlineColor))
	{
		LastGimmickOutlineColor = OutlineColor;
		GimmickStatusDrawBrush.OutlineSettings.Color = OutlineColor;
		GimmickStatusImage->SetBrush(GimmickStatusDrawBrush);
	}

	GimmickStatusImage->SetColorAndOpacity(Color);
	GimmickStatusImage->SetVisibility(Color.A > KINDA_SMALL_NUMBER ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UShooterMapWidget::ApplyFullMapViewportShape()
{
	if (MapMode != EShooterMapMode::FullMap || FullMapSizeBox == nullptr)
	{
		return;
	}

	// WBP SizeBox에 크기를 지정하지 않았으면 처음 배치된 실제 크기를 기준으로 삼는다.
	if (FullMapBaseSize.X <= 0.f || FullMapBaseSize.Y <= 0.f)
	{
		FullMapBaseSize = FullMapSizeBox->GetCachedGeometry().GetLocalSize();
	}

	const FVector2D ShapeSize = bHasActiveZone ? ActiveZoneMax - ActiveZoneMin : PlayableAreaMax - PlayableAreaMin;
	if (FullMapBaseSize.X <= 0.f || FullMapBaseSize.Y <= 0.f || ShapeSize.X <= 0.f || ShapeSize.Y <= 0.f)
	{
		return;
	}

	// 기본 창(정사각형)과 넓이가 같고 비율은 보여줄 영역과 같은 크기. 가로로 긴 구역이면 옆으로 늘어나고 위아래가 줄어든다.
	const float BaseArea = FullMapBaseSize.X * FullMapBaseSize.Y;
	const float Aspect = ShapeSize.X / ShapeSize.Y;
	FVector2D TargetSize = FVector2D(FMath::Sqrt(BaseArea * Aspect), FMath::Sqrt(BaseArea / Aspect)) * FullMapSizeScale;

	// 화면(또는 지정한 최대 크기)을 넘으면 비율을 유지한 채 그 안에 맞춘다.
	FVector2D MaxSize = FullMapMaxViewportSize;
	if (MaxSize.X <= 0.f || MaxSize.Y <= 0.f)
	{
		const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
		MaxSize = UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(ViewportScale, KINDA_SMALL_NUMBER) * FullMapMaxScreenFraction;
	}
	if (MaxSize.X > 0.f && MaxSize.Y > 0.f)
	{
		TargetSize *= FMath::Min(1.f, FMath::Min(MaxSize.X / TargetSize.X, MaxSize.Y / TargetSize.Y));
	}

	// 1배가 곧 영역이 창을 꽉 채운 상태가 된다.
	FullMapViewportSize = TargetSize;
	FullMapSizeBox->SetWidthOverride(FullMapViewportSize.X);
	FullMapSizeBox->SetHeightOverride(FullMapViewportSize.Y);
}

void UShooterMapWidget::ApplyZoneMask()
{
	if (!bHasActiveZone)
	{
		for (UImage* Image : ZoneMaskImages)
		{
			if (Image)
			{
				Image->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		return;
	}

	while (ZoneMaskImages.Num() < 4)
	{
		UImage* Image = CreateMarkerImage(ShooterMapWidgetRules::ZoneMaskZOrder);
		if (Image == nullptr)
		{
			return;
		}
		ZoneMaskImages.Add(Image);
	}

	// 구역 바깥을 위/아래(가로 전체) + 왼쪽/오른쪽(구역 높이만큼)으로 나눠 겹치지 않게 덮는다.
	const FBox2D Rects[] = {
		FBox2D(FVector2D::ZeroVector, FVector2D(MapContentSize.X, ActiveZoneMin.Y)),
		FBox2D(FVector2D(0.f, ActiveZoneMax.Y), MapContentSize),
		FBox2D(FVector2D(0.f, ActiveZoneMin.Y), FVector2D(ActiveZoneMin.X, ActiveZoneMax.Y)),
		FBox2D(FVector2D(ActiveZoneMax.X, ActiveZoneMin.Y), FVector2D(MapContentSize.X, ActiveZoneMax.Y)),
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rects); ++Index)
	{
		UImage* Image = ZoneMaskImages[Index];
		if (UCanvasPanelSlot* MaskSlot = Image ? Cast<UCanvasPanelSlot>(Image->Slot) : nullptr)
		{
			// 마커와 달리 Content 좌표에 고정 크기로 깐다. 확대/회전은 Content 변환을 그대로 따른다.
			MaskSlot->SetAlignment(FVector2D::ZeroVector);
			MaskSlot->SetAutoSize(false);
			MaskSlot->SetPosition(Rects[Index].Min);
			MaskSlot->SetSize(Rects[Index].GetSize());
			Image->SetColorAndOpacity(OutsideZoneColor);
			Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
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
