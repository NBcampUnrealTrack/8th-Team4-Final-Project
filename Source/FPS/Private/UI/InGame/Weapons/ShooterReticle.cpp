// ShooterReticle.cpp
#include "UI/InGame/Weapons/ShooterReticle.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Animation/WidgetAnimation.h" // (Modified: 히트마커 페이드 효과 추가)
#include "Interfaces/PlayerInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/CombatComponent.h"
#include "Player/ShooterPlayerController.h"
#include "UnrealClient.h"
#include "Settings/ShooterGameUserSettings.h"
#include "Tags/GameplayTags.h"
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

static void SetSlotRenderOffset(UWidget* Widget, const FVector2D& Offset)
{
	if (!Widget)
	{
		return;
	}

	if (Widget->GetRenderTransform().Translation.Equals(Offset, 0.05f))
	{
		return;
	}

	Widget->SetRenderTranslation(Offset);
}

void UShooterReticle::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ConfigureCenteredCanvasSlot(Image_Top);
	ConfigureCenteredCanvasSlot(Image_Bottom);
	ConfigureCenteredCanvasSlot(Image_Left);
	ConfigureCenteredCanvasSlot(Image_Right);
	ConfigureCenteredCanvasSlot(Image_Center);

	SetSlotPosition(Image_Top, FVector2D::ZeroVector);
	SetSlotPosition(Image_Bottom, FVector2D::ZeroVector);
	SetSlotPosition(Image_Left, FVector2D::ZeroVector);
	SetSlotPosition(Image_Right, FVector2D::ZeroVector);

	// Top/Bottom도 Left/Right와 같은(가로로 긴) CrossSide 브러시를 그대로 쓰기로 했으므로,
	// 세로 방향으로 보이도록 자기 중심을 기준으로 90도 돌려준다. Left/Right는 원본 그대로 둔다.
	SetSlotPosition(Image_Center, FVector2D::ZeroVector);

	auto RotateInPlace = [](UImage* Img, const float AngleDegrees)
	{
		if (!Img)
		{
			return;
		}
		Img->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Img->SetRenderTransformAngle(AngleDegrees);
	};
	RotateInPlace(Image_Top, 90.f);
	RotateInPlace(Image_Bottom, 90.f);

	// 히트마커 기능. 크로스헤어와 같은 중앙 위치에 겹쳐서 뜬다(반투명 처리는 HandleHitMarker에서).
	ConfigureCenteredCanvasSlot(Image_HitMarker);
	SetSlotPosition(Image_HitMarker, FVector2D::ZeroVector);

	bHitMarkerActive = false;
	if (Image_HitMarker)
	{
		Image_HitMarker->SetVisibility(ESlateVisibility::Hidden);
	}

	// 킬 이펙트 기능. 히트마커와 같은 중앙 위치에 겹쳐서 뜬다(반투명 처리는 HandleKillEffect에서).
	ConfigureCenteredCanvasSlot(Image_KillEffect);
	SetSlotPosition(Image_KillEffect, FVector2D::ZeroVector);

	bKillEffectActive = false;
	if (Image_KillEffect)
	{
		Image_KillEffect->SetVisibility(ESlateVisibility::Hidden);
	}

	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->OnHitMarker.AddUObject(this, &ThisClass::HandleHitMarker);
		PC->OnKillEffect.AddUObject(this, &ThisClass::HandleKillEffect);
	}

	// 페이드 애니메이션이 끝나는 시점에 히트마커 상태를 종료한다.
	if (Anim_HitMarker)
	{
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &ThisClass::HandleHitMarkerAnimationFinished);
		BindToAnimationFinished(Anim_HitMarker, FinishedEvent);
	}

	// 페이드 애니메이션이 끝나는 시점에 킬 이펙트 상태를 종료한다.
	if (Anim_KillEffect)
	{
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &ThisClass::HandleKillEffectAnimationFinished);
		BindToAnimationFinished(Anim_KillEffect, FinishedEvent);
	}

}

void UShooterReticle::NativeConstruct()
{
	Super::NativeConstruct();
	PromoteSniperScopeToViewportLayer();

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		HandlePossessedPawnChanged(nullptr, PC->GetPawn());
	}

	// 동적 크로스헤어 On/Off 등 설정이 바뀌면 간격을 즉시 다시 계산한다.
	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		CrosshairSettingsHandle = Settings->OnCrosshairSettingsChanged.AddUObject(this, &ThisClass::RefreshSpreadOffsets);
	}

	// ComputeSpreadRadius가 뷰포트 크기에 의존하므로 창 크기/해상도 변경 시에도 다시 계산한다.
	ViewportResizedHandle = FViewport::ViewportResizedEvent.AddUObject(this, &ThisClass::HandleViewportResized);

	RefreshSpreadOffsets();
}

void UShooterReticle::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UCombatComponent* NewCombat = nullptr;
	if (NewPawn && NewPawn->Implements<UPlayerInterface>())
	{
		NewCombat = IPlayerInterface::Execute_GetCombatComponent(NewPawn);
	}

	BindToCombatComponent(NewCombat);
}

void UShooterReticle::BindToCombatComponent(UCombatComponent* NewCombat)
{
	if (CachedCombat.Get() == NewCombat)
	{
		return;
	}

	if (UCombatComponent* OldCombat = CachedCombat.Get())
	{
		OldCombat->OnCrosshairSpreadInputsChanged.Remove(CrosshairSpreadInputsHandle);
	}
	CrosshairSpreadInputsHandle.Reset();

	CachedCombat = NewCombat;

	if (NewCombat)
	{
		CrosshairSpreadInputsHandle = NewCombat->OnCrosshairSpreadInputsChanged.AddUObject(this, &ThisClass::RefreshSpreadOffsets);
	}

	// 구독 직후에는 CombatComponent가 "값이 안 바뀌었다"고 판단해 알리지 않을 수 있으므로 한 번 직접 갱신한다.
	RefreshSpreadOffsets();
}

void UShooterReticle::HandleViewportResized(FViewport* Viewport, uint32 Unused)
{
	RefreshSpreadOffsets();
}

void UShooterReticle::RefreshSpreadOffsets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UShooterReticle::RefreshSpreadOffsets);

	// 4방향 모두 브러시 자체가 이미 올바른 방향을 향하고 있으므로, 회전 없이 중심에서 반경만큼
	// 축을 따라 밀어내기만 하면 된다. Top/Bottom은 박스 크기 차이를 보정하기 위해 반경을 줄이고,
	// VerticalGroupOffset으로 둘을 한 축으로 묶어 미세 위치를 맞춘다.
	const float Radius = FMath::Max(ComputeSpreadRadius(), MinSpreadRadius);
	const float VerticalRadius = Radius * VerticalSpreadRadiusScale;
	SetSlotRenderOffset(Image_Right, HorizontalGroupOffset + FVector2D(Radius, 0.f));
	SetSlotRenderOffset(Image_Left, HorizontalGroupOffset + FVector2D(-Radius, 0.f));
	SetSlotRenderOffset(Image_Top, VerticalGroupOffset + FVector2D(0.f, -VerticalRadius - TopBarLift));
	SetSlotRenderOffset(Image_Bottom, VerticalGroupOffset + FVector2D(0.f, VerticalRadius));
}

void UShooterReticle::PromoteSniperScopeToViewportLayer()
{
	if (!WBP_SniperScope || WBP_SniperScope->IsInViewport())
	{
		return;
	}

	// Scope는 Reticle의 자식으로 그리면 전체화면 검은 마스크가 Main HUD까지 덮는다.
	// 같은 위젯 인스턴스를 부모에서 분리해 Root HUD보다 한 단계 아래 Viewport 레이어로 옮긴다.
	// Scope 자체의 기존 Blueprint Visibility/ADS 로직은 그대로 유지된다.
	WBP_SniperScope->RemoveFromParent();
	WBP_SniperScope->AddToViewport(SniperScopeViewportZOrder);
}

void UShooterReticle::RemoveSniperScopeFromViewportLayer()
{
	if (WBP_SniperScope && WBP_SniperScope->IsInViewport())
	{
		WBP_SniperScope->RemoveFromParent();
	}
}

void UShooterReticle::NativeDestruct()
{
	RemoveSniperScopeFromViewportLayer();
	// 델리게이트 해제
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->OnHitMarker.RemoveAll(this);
		PC->OnKillEffect.RemoveAll(this);
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	BindToCombatComponent(nullptr);

	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		Settings->OnCrosshairSettingsChanged.Remove(CrosshairSettingsHandle);
	}
	CrosshairSettingsHandle.Reset();

	FViewport::ViewportResizedEvent.Remove(ViewportResizedHandle);
	ViewportResizedHandle.Reset();

	// 히트마커/킬이펙트 등 이 위젯이 건 타이머를 한 번에 정리한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	bHitMarkerActive = false;
	bKillEffectActive = false;

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
	
	const float SpreadDegrees = GetSpreadDegrees();
	if (SpreadDegrees <= UE_KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	// 현재 총의 탄퍼짐 각도를 실제 화면상의 크로스헤어 간격으로 변환한다.
	// 화면 중심에서 그 점깢의 거리가 곧 크로스헤어 반경이 된다.
	const float SpreadRadiusRads = FMath::DegreesToRadians(SpreadDegrees);
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

bool UShooterReticle::IsHitMarkerActive() const
{
	return bHitMarkerActive;
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
	if (!Weapon)
	{
		return 0.f;
	}

	// 스나이퍼 조준선은 실제 탄퍼짐(비ADS 강제 탄퍼짐 포함)과 무관하게 항상 벌어지지 않게 고정한다.
	// 표시만 고정할 뿐, 실제 탄 분산은 AWeaponBase::GetCurrentSpreadDegrees()가 그대로 적용한다.
	if (Weapon->GetWeaponTag() == RogueTags::Weapon_Type_Sniper)
	{
		return 0.f;
	}

	return Weapon->GetCurrentSpreadDegrees();
}

// 히트마커
void UShooterReticle::HandleHitMarker(const ERogueHitMarkerType Type, const bool bKill)
{
	if (!Image_HitMarker)
	{
		return;
	}

	bHitMarkerActive = true;
	LastHitMarkerType = Type;
	bLastHitMarkerKill = bKill;

	// 알파를 HitMarkerOpacity로 낮춰서 크로스헤어와 겹쳐도 뒤가 비쳐 보이게 한다.
	FLinearColor Color = ResolveHitMarkerColor(Type, bKill);
	Color.A = FMath::Clamp(HitMarkerOpacity, 0.f, 1.f);

	Image_HitMarker->SetColorAndOpacity(Color);
	Image_HitMarker->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 이전 타이머가 새 히트마커를 조기에 숨기지 않도록 갱신한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitMarkerTimerHandle);
	}

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

// 히트마커 자체만 제거한다. 기본 크로스헤어 Visibility는 Blueprint가 결정한다.
void UShooterReticle::HideHitMarker()
{
	bHitMarkerActive = false;

	if (Image_HitMarker)
	{
		Image_HitMarker->SetVisibility(ESlateVisibility::Hidden);
	}
}

// Anim_HitMarker 재생 종료 시 호출
void UShooterReticle::HandleHitMarkerAnimationFinished()
{
	HideHitMarker();
}

// 킬 이펙트. 크기 변화(크게 등장 -> 작아지며 페이드)는 Anim_KillEffect의 RenderTransform Scale +
// Render Opacity 트랙이 담당하고, 여기서는 표시 상태/알파(반투명)만 처리한다.
void UShooterReticle::HandleKillEffect()
{
	if (!Image_KillEffect)
	{
		return;
	}

	bKillEffectActive = true;

	// 알파를 KillEffectOpacity로 낮춰서 크로스헤어/히트마커와 겹쳐도 뒤가 비쳐 보이게 한다.
	Image_KillEffect->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(KillEffectOpacity, 0.f, 1.f)));
	Image_KillEffect->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 이전 타이머가 새 킬 이펙트를 조기에 숨기지 않도록 갱신한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(KillEffectTimerHandle);
	}

	// 애니메이션이 있으면 그걸로 등장/축소/페이드, 없으면 즉시 켜고 KillEffectDuration 뒤에 끈다.
	if (Anim_KillEffect)
	{
		PlayAnimation(Anim_KillEffect);
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			KillEffectTimerHandle, this, &ThisClass::HideKillEffect, KillEffectDuration, false);
	}
}

// 킬 이펙트 자체만 제거한다.
void UShooterReticle::HideKillEffect()
{
	bKillEffectActive = false;

	if (Image_KillEffect)
	{
		Image_KillEffect->SetVisibility(ESlateVisibility::Hidden);
	}
}

// Anim_KillEffect 재생 종료 시 호출
void UShooterReticle::HandleKillEffectAnimationFinished()
{
	HideKillEffect();
}

// 헤드샷과 처치는 부위 구분보다 우선해서 빨간색으로 강조한다. 그 외(바디/쉴드)는 부위별 색상을 쓴다.
// 색상 대신/추가로 아이콘까지 바꾸고 싶다면 Blueprint에서 GetHitMarkerType()/WasHitMarkerKill()을 읽어 처리한다.
FLinearColor UShooterReticle::ResolveHitMarkerColor(const ERogueHitMarkerType Type, const bool bKill)
{
	if (bKill || Type == ERogueHitMarkerType::Headshot)
	{
		return FLinearColor::Red;
	}

	switch (Type)
	{
	case ERogueHitMarkerType::Shield:
		return FLinearColor(0.3f, 0.75f, 1.f); // 하늘색
	case ERogueHitMarkerType::Body:
	default:
		return FLinearColor::White;
	}
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

