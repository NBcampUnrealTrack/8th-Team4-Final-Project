// ShooterReticle.cpp
#include "UI/InGame/Weapons/ShooterReticle.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Interfaces/PlayerInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
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

bool UShooterReticle::IsAiming() const
{
	const AWeaponBase* Weapon = FindEquippedWeapon();
	return IsValid(Weapon) && Weapon->IsADS();
}

FGameplayTag UShooterReticle::GetEquippedWeaponTag() const
{
	const AWeaponBase* Weapon = FindEquippedWeapon();
	return IsValid(Weapon) ? Weapon->GetWeaponTag() : FGameplayTag();
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

