// TeammateIndicatorEntry.h
#include "UI/InGame/HUD/TeammateIndicatorEntry.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "CollisionQueryParams.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Camera/PlayerCameraManager.h"

void UTeammateIndicatorEntry::NativeConstruct()
{
	Super::NativeConstruct();
	
	// Slate가 재구축 시 구독만 끊겨 있으므로 되살린다
	if (AShooterPlayerState* PS = BoundPlayerState.Get())
	{
		bCachedIsAlive = PS->IsAlive();
	}
	
	BindToPlayerState();
}

void UTeammateIndicatorEntry::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	UpdateScreenPositionAndVisibility();
}

void UTeammateIndicatorEntry::NativeDestruct()
{
	UnbindFromPlayerState();
	
	Super::NativeDestruct();
}

void UTeammateIndicatorEntry::InitializeIndicator(UCanvasPanel* InOwnerCanvas, UCanvasPanelSlot* InCanvasSlot)
{
	OwnerCanvas = InOwnerCanvas;
	CachedCanvasSlot = InCanvasSlot;
}

void UTeammateIndicatorEntry::UpdateScreenPositionAndVisibility()
{
	// 1. 초기 반환은 전부 Panel_IndicatorVisual만 접는다.
	auto HideVisual = [this]()
	{
		if (Panel_IndicatorVisual)
		{
			Panel_IndicatorVisual->SetVisibility(ESlateVisibility::Collapsed);
		}
	};

	if (bCachedIsAlive == false)
	{
		HideVisual();
		return;
	}
	
	FVector HeadLocation = FVector::ZeroVector;
	AActor* TargetActor = nullptr;
	if (GetTargetHeadLocation(HeadLocation, TargetActor) == false)
	{
		HideVisual();
		return;
	}
	
	APlayerController* PC = GetOwningPlayer();
	if (IsValid(PC) == false || CachedCanvasSlot.IsValid() == false || OwnerCanvas.IsValid() == false)
	{
		HideVisual();
		return;
	}
	
	// 첫 Tick에서는 GetCachedGeometry()가 크기 0 기본값을 돌려주기때문에
	// 그 값으로 AbsoluteToLocal을 하면 좌표가 쓰레기ㄱㅏ 되므로 그 Tick은 건너뛴다.
	const FGeometry CanvasGeom = OwnerCanvas->GetCachedGeometry();
	if (CanvasGeom.GetLocalSize().IsNearlyZero())
	{
		HideVisual();
		return;
	}
	
	// 2. 전체 게임 Viewport 기준의 2D 좌표를 얻는다.
	// 뒤에서 GetViewportWidgetGeometry()도 전체 Viewport 기준으로 사용하므로 false로 맞춘다.
	FVector2D ViewportLocalPos = FVector2D::ZeroVector;
	const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PC,
		HeadLocation,
		ViewportLocalPos,
		false);

	// 카메라 뒤
	if (bProjected == false)
	{
		HideVisual();
		return;
	}
	
	// 3. bProjected는 카메라 뒤만 걸러내 줌으로
	// 카메라 앞이면서 FOV 밖인 경우는 true인 채로 좌표만 범위를 벗어나므로 직접 거름
	const FGeometry ViewportGeom = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	const FVector2D ViewportLocalSize = ViewportGeom.GetLocalSize();
	if (ViewportLocalPos.X < 0.f || ViewportLocalPos.Y < 0.f 
		|| ViewportLocalPos.X > ViewportLocalSize.X 
		|| ViewportLocalPos.Y > ViewportLocalSize.Y)
	{
		HideVisual();
		return;
	}
	
	// 4. Viewport 기준 좌표를 실제로 배치할 Canvas 기준 좌표로 바꾼다.
	// 중간에 ScaleBox나 다른 UI 변환이 있을 수 있으므로 Absolute 좌표를 한 번 거친다.
	const FVector2D AbsolutePos = ViewportGeom.LocalToAbsolute(ViewportLocalPos);
	const FVector2D OurLocalPos = CanvasGeom.AbsoluteToLocal(AbsolutePos);

	// 5. 계산된 Canvas 위치로 인디케이터를 옮긴 뒤 보여준다.
	// 먼저 보여주면 이전 위치나 기본 위치가 한 프레임 보일 수 있다.
	CachedCanvasSlot->SetPosition(OurLocalPos);
	
	if (Panel_IndicatorVisual)
	{
		Panel_IndicatorVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	
	if (Text_Nickname)
	{
		const bool bHeadVisible = HasLineOfSightToTargetHead(HeadLocation, TargetActor);
		
		Text_Nickname->SetVisibility(bHeadVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UTeammateIndicatorEntry::GetTargetHeadLocation(FVector& OutHeadLocation, AActor*& OutTargetActor) const
{
	AShooterPlayerState* PS = BoundPlayerState.Get();
	if (IsValid(PS) == false)
	{
		return false;
	}
	
	APawn* TargetPawn = PS->GetPawn();
	if (IsValid(TargetPawn) == false)
	{
		return false;
	}
	
	const ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn);
	const USkeletalMeshComponent* Mesh = IsValid(TargetCharacter) ? TargetCharacter->GetMesh() : nullptr;
	if (IsValid(Mesh) == false)
	{
		UE_LOG(LogTemp, Warning, TEXT("TeammateIndicatorEntry Mesh가 없음"));
		return false;
	}
	
	OutHeadLocation = Mesh->GetSocketLocation(HeadSocketName) + FVector(0.f, 0.f, HeadWorldZOffset);
	OutTargetActor = TargetPawn;
	
	return true;
}

void UTeammateIndicatorEntry::BindToPlayerState()
{
	if (AShooterPlayerState* PS = BoundPlayerState.Get())
	{
		PS->OnPlayerAliveStateChanged.AddUniqueDynamic(this, &ThisClass::HandleAliveStateChanged);
	}
}

void UTeammateIndicatorEntry::UnbindFromPlayerState()
{
	if (BoundPlayerState.IsValid())
	{
		BoundPlayerState->OnPlayerAliveStateChanged.RemoveAll(this);
	}
	
	// Entry가 재사용될 수 있으므로 대상 PlayerState는 유지한다.
	// 여기서는 델리게이트만 해제한다.
}

void UTeammateIndicatorEntry::HandleAliveStateChanged(bool bNewIsAlive)
{
	bCachedIsAlive = bNewIsAlive;
}

bool UTeammateIndicatorEntry::HasLineOfSightToTargetHead(const FVector& HeadLocation, const AActor* TargetActor) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = GetOwningPlayer();
	
	// 닉네임 가림 판정은 실제 화면을 보고 있는 카메라 위치를 기준으로 한다.
	// Pawn 위치는 사망 후 래그돌 상태에서 실제 카메라 위치와 어긋날 수 있다.
	const APlayerCameraManager* CameraManager = IsValid(PC) ? PC->PlayerCameraManager : nullptr;
	
	if (IsValid(World) == false || IsValid(CameraManager) == false)
	{
		return false;
	}
	
	const FVector TraceStart = CameraManager->GetCameraLocation();
	
	// 자기 캐릭터와 대상 팀원 자체는 벽으로 판정하지않음
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TeammateIndicatorVisibility), false, PC->GetPawn());
	QueryParams.AddIgnoredActor(TargetActor);
	
	FHitResult VisibilityHit;
	return World->LineTraceSingleByChannel(
		VisibilityHit,
		TraceStart,
		HeadLocation,
		VisibilityTraceChannel,
		QueryParams) == false;
}

void UTeammateIndicatorEntry::SetPlayerState(AShooterPlayerState* NewPlayerState)
{
	// 이전 대상의 생사 구독 먼저 끊음
	UnbindFromPlayerState();
	
	BoundPlayerState = NewPlayerState;
	
	if (NewPlayerState == nullptr)
	{
		if (Text_Nickname)
		{
			Text_Nickname->SetText(FText::GetEmpty());
		}
		
		// 이전 대상의 Jack 상태가 남지 않게 되돌림
		SetJackCarrierVisible(false);
		
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	
	// Bound 상태. 이후 계속 Tick으로 폴링해야 화면 밖에서 돌아올 수 있음
	SetVisibility(ESlateVisibility::HitTestInvisible);
	
	// 여기서 표시하지않고 첫 투영이 성공한 Tick에서만 표시
	if (Panel_IndicatorVisual)
	{
		Panel_IndicatorVisual->SetVisibility(ESlateVisibility::Collapsed);
	}
	
	if (Text_Nickname)
	{
		Text_Nickname->SetText(FText::FromString(NewPlayerState->GetPlayerName()));
	}
	
	// 구독보다 먼저 상태를 체크한다.
	bCachedIsAlive = NewPlayerState->IsAlive();
	
	BindToPlayerState();
}

void UTeammateIndicatorEntry::SetJackCarrierVisible(bool bVisible)
{
	if (Image_JackCarrier)
	{
		Image_JackCarrier->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

AShooterPlayerState* UTeammateIndicatorEntry::GetBoundPlayerState() const
{
	return BoundPlayerState.Get();
}
