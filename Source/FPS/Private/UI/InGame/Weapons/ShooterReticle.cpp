// ShooterReticle.cpp
#include "UI/InGame/Weapons/ShooterReticle.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h" // (Modified: 히트마커 페이드 효과 추가)
#include "Interfaces/PlayerInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Player/ShooterPlayerController.h"
#include "Settings/ShooterGameUserSettings.h"
#include "Weapon/WeaponBase.h"

static void ConfigureCenteredCanvasSlot(UWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		Slot->SetAlignment(FVector2D(0.5f, 0.5f));
	}
}

static void SetSlotPosition(UWidget* Widget, const FVector2D& Position)
{
	if (!Widget)
	{
		return;
	}
	
	if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		Slot->SetPosition(Position);
	}
}

void UShooterReticle::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ConfigureCenteredCanvasSlot(Image_Top);
	ConfigureCenteredCanvasSlot(Image_Bottom);
	ConfigureCenteredCanvasSlot(Image_Left);
	ConfigureCenteredCanvasSlot(Image_Right);
	ConfigureCenteredCanvasSlot(Image_Center);

	SetSlotPosition(Image_Center, FVector2D::ZeroVector);

	// 히트마커 기능
	if (Text_HitMarker)
	{
		Text_HitMarker->SetVisibility(ESlateVisibility::Hidden);
	}

	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->OnHitMarker.AddUObject(this, &ThisClass::HandleHitMarker);
	}

	// (Modified: 히트마커 페이드 효과 추가) 페이드 애니메이션이 끝나는 시점에 크로스헤어를 복원
	if (Anim_HitMarker)
	{
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &ThisClass::HandleHitMarkerAnimationFinished);
		BindToAnimationFinished(Anim_HitMarker, FinishedEvent);
	}
}

void UShooterReticle::NativeDestruct()
{
	// 델리게이트 해제
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->OnHitMarker.RemoveAll(this);
	}

	Super::NativeDestruct();
}

float UShooterReticle::ComputeSpreadRadius() const
{
	// 카메라 앞 쪽으로 가상의 먼 거리를 잡는다.
	const float LongShotDistance = 10000.f;
	
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->PlayerCameraManager)
	{
		return 0.f;
	}
	
	// 현재 총의 탄퍼짐 각도를 실제 화면상의 크로스헤어 간격으로 변환한다.
	// 화면 중심에서 그 점깢의 거리가 곧 크로스헤어 반경이 된다.
	const float SpreadRadiusRads = FMath::DegreesToRadians(GetSpreadDegrees());
	const float SpreadRadiusAtDistance = FMath::Tan(SpreadRadiusRads) * LongShotDistance;
	
	FVector CamPos;
	FRotator CamOrient; 
	PC->PlayerCameraManager->GetCameraViewPoint(CamPos, CamOrient);
	
	const FVector CamForwDir = CamOrient.RotateVector(FVector::ForwardVector);
	const FVector CamUpDir = CamOrient.RotateVector(FVector::UpVector);
	const FVector OffsetTargetAtDistance = CamPos + (CamForwDir * LongShotDistance) + (CamUpDir * SpreadRadiusAtDistance);
	
	FVector2D OffsetTargetInScreenspace;
	if (PC->ProjectWorldLocationToScreen(OffsetTargetAtDistance, OffsetTargetInScreenspace, true))
	{
		int32 ViewportSizeX = 0, ViewportSizeY = 0;
		PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
		const FVector2D ScreenCenter(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f);
		return (OffsetTargetInScreenspace - ScreenCenter).Length() * SpreadRadiusScale;
	}
	return 0.f;
}

bool UShooterReticle::IsPerfectAccuracy() const
{
	return GetSpreadDegrees() <= UE_KINDA_SMALL_NUMBER;
}

void UShooterReticle::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	const float Radius = FMath::Max(ComputeSpreadRadius(), MinSpreadRadius);
	
	SetSlotPosition(Image_Top,    FVector2D(0.f, -Radius));
	SetSlotPosition(Image_Bottom, FVector2D(0.f,  Radius));
	SetSlotPosition(Image_Left,   FVector2D(-Radius, 0.f));
	SetSlotPosition(Image_Right,  FVector2D( Radius, 0.f));
}

float UShooterReticle::GetSpreadDegrees() const
{
	const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	if (Settings && !Settings->IsDynamicCrosshairEnabled())
	{
		// 정적 크로스헤어 설정: 실제 탄퍼짐과 무관하게 항상 0(=MinSpreadRadius로 고정)으로 취급한다.
		return 0.f;
	}

	const AWeaponBase* Weapon = FindEquippedWeapon();
	return Weapon ? Weapon->GetCurrentSpreadDegrees() : 0.f;
}

// 히트마커
void UShooterReticle::SetCrosshairArmsVisible(const bool bVisible)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;

	if (Image_Top)    { Image_Top->SetVisibility(NewVisibility); }
	if (Image_Bottom) { Image_Bottom->SetVisibility(NewVisibility); }
	if (Image_Left)   { Image_Left->SetVisibility(NewVisibility); }
	if (Image_Right)  { Image_Right->SetVisibility(NewVisibility); }
	if (Image_Center) { Image_Center->SetVisibility(NewVisibility); }
}

void UShooterReticle::HandleHitMarker(const bool bHeadshot)
{
	if (!Text_HitMarker)
	{
		return;
	}

	SetCrosshairArmsVisible(false);

	Text_HitMarker->SetColorAndOpacity(bHeadshot ? FLinearColor::Red : FLinearColor::White);
	Text_HitMarker->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 애니메이션이 있으면 그걸로 페이드, 없으면 즉시 켜고 HitMarkerDuration 뒤에 끈다.
	if (Anim_HitMarker)
	{
		PlayAnimation(Anim_HitMarker);
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HitMarkerTimerHandle, this, &ThisClass::HideHitMarker, HitMarkerDuration, false);
	}
}

// 히트마커 제거 - 크로스헤어 복원
void UShooterReticle::HideHitMarker()
{
	if (Text_HitMarker)
	{
		Text_HitMarker->SetVisibility(ESlateVisibility::Hidden);
	}

	SetCrosshairArmsVisible(true);
}

// (Modified: 히트마커 페이드 효과 추가) Anim_HitMarker 재생 종료 시 호출
void UShooterReticle::HandleHitMarkerAnimationFinished()
{
	HideHitMarker();
}

AWeaponBase* UShooterReticle::FindEquippedWeapon() const
{
	APawn* Pawn = GetOwningPlayerPawn();
	if (!Pawn)
	{
		return nullptr;
	}
	
	if (Pawn->Implements<UPlayerInterface>())
	{
		if (AWeaponBase* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(Pawn))
		{
			return Weapon;
		}
	}
	
	return nullptr;
}

