#include "UI/InGame/Weapons/ShooterReticle.h"

#include "Animation/WidgetAnimation.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/CombatComponent.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/PlayerInterface.h"
#include "Player/ShooterPlayerController.h"
#include "Settings/ShooterGameUserSettings.h"
#include "Tags/GameplayTags.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Weapon/WeaponBase.h"

namespace
{
	// UI 단위: 복귀 종료 판정과 눈에 보이지 않는 미세 이동 생략에 함께 사용한다.
	constexpr float SpreadSettleTolerance = 0.05f;
	constexpr float SpreadProjectionDistance = 10000.f;
	// 한 번의 프레임 지연으로 복귀 구간 전체를 건너뛰지 않도록 한다.
	constexpr double MaxSpreadAnimationStep = 0.05;

	void InitializeCenteredCanvasSlot(UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}

		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			Slot->SetAlignment(FVector2D(0.5f, 0.5f));
			Slot->SetPosition(FVector2D::ZeroVector);
		}
	}

	void SetSlotRenderOffset(UWidget* Widget, const FVector2D& Offset)
	{
		if (Widget && !Widget->GetRenderTransform().Translation.Equals(Offset, SpreadSettleTolerance))
		{
			Widget->SetRenderTranslation(Offset);
		}
	}
}

void UShooterReticle::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UImage* CrosshairImages[] = { Image_Top.Get(), Image_Bottom.Get(), Image_Left.Get(), Image_Right.Get(), Image_Center.Get() };
	for (UImage* Image : CrosshairImages)
	{
		InitializeCenteredCanvasSlot(Image);
	}

	// 위/아래 막대는 같은 가로 브러시를 중심 기준 90도 회전해 사용한다.

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
	InitializeCenteredCanvasSlot(Image_HitMarker);

	bHitMarkerActive = false;
	if (Image_HitMarker)
	{
		Image_HitMarker->SetVisibility(ESlateVisibility::Hidden);
	}

	// 킬 이펙트 기능. 히트마커와 같은 중앙 위치에 겹쳐서 뜬다(반투명 처리는 HandleKillEffect에서).
	InitializeCenteredCanvasSlot(Image_KillEffect);

	bKillEffectActive = false;
	if (Image_KillEffect)
	{
		Image_KillEffect->SetVisibility(ESlateVisibility::Hidden);
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
	bSpreadPresentationActive = true;

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->OnHitMarker.AddUObject(this, &ThisClass::HandleHitMarker);
		PC->OnKillEffect.AddUObject(this, &ThisClass::HandleKillEffect);
		ViewPresentationHandle = PC->OnViewPresentationChanged.AddUObject(this, &ThisClass::HandleCrosshairPresentationChanged);
	}

	// 설정 변경 이벤트 한 번으로 색상과 동적 간격을 갱신한다.
	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		CachedCrosshairSettings = Settings;
		CrosshairSettingsHandle = Settings->OnCrosshairSettingsChanged.AddUObject(this, &ThisClass::HandleCrosshairPresentationChanged);
	}

	// ComputeSpreadRadius가 뷰포트 크기에 의존하므로 창 크기/해상도 변경 시에도 다시 계산한다.
	ViewportResizedHandle = FViewport::ViewportResizedEvent.AddUObject(this, &ThisClass::HandleViewportResized);

	// 설정과 이벤트 연결을 마친 후 초기 표시를 한 번만 갱신한다.
	HandlePossessedPawnChanged(nullptr, GetOwningPlayerPawn());
}

void UShooterReticle::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UCombatComponent* NewCombat = nullptr;
	if (NewPawn && NewPawn->Implements<UPlayerInterface>())
	{
		NewCombat = IPlayerInterface::Execute_GetCombatComponent(NewPawn);
	}

	BindToCombatComponent(NewCombat);
	HandleCrosshairPresentationChanged();
}

void UShooterReticle::BindToCombatComponent(UCombatComponent* NewCombat)
{
	if (CachedCombat.Get() == NewCombat)
	{
		return;
	}

	UnbindFromCombatComponent();
	CachedCombat = NewCombat;

	if (NewCombat)
	{
		CrosshairSpreadInputsHandle = NewCombat->OnCrosshairSpreadInputsChanged.AddUObject(this, &ThisClass::RefreshSpreadOffsets);
		CrosshairTargetHandle = NewCombat->OnCrosshairTargetChanged.AddUObject(this, &ThisClass::HandleCrosshairTargetChanged);
	}
}

void UShooterReticle::UnbindFromCombatComponent()
{
	if (UCombatComponent* Combat = CachedCombat.Get())
	{
		Combat->OnCrosshairSpreadInputsChanged.Remove(CrosshairSpreadInputsHandle);
		Combat->OnCrosshairTargetChanged.Remove(CrosshairTargetHandle);
	}
	CrosshairSpreadInputsHandle.Reset();
	CrosshairTargetHandle.Reset();
	CachedCombat.Reset();
}

void UShooterReticle::HandleCrosshairPresentationChanged()
{
	ResetSpreadPresentation();
	RefreshCrosshairColor();
	RefreshSpreadOffsets();
}

void UShooterReticle::HandleCrosshairTargetChanged(bool /*bAimingAtEnemy*/)
{
	RefreshCrosshairColor();
}

void UShooterReticle::RefreshCrosshairColor()
{
	const UShooterGameUserSettings* Settings = GetCrosshairSettings();
	FLinearColor Color = Settings ? Settings->GetCrosshairColor() : FLinearColor::White;
	const UCombatComponent* Combat = CachedCombat.Get();
	const APlayerController* PC = GetOwningPlayer();
	if (Settings && Settings->IsEnemyCrosshairColorEnabled() && Combat && Combat->IsAimingAtEnemy()
		&& PC && PC->IsLocalController() && PC->GetPawn() == Combat->GetOwner()
		&& PC->GetViewTarget() == Combat->GetOwner())
	{
		// 저장된 RGBA는 그대로 두고 화면에 표시하는 RGB만 바꾼다.
		const float SavedAlpha = Color.A;
		Color = EnemyCrosshairColor;
		Color.A = SavedAlpha;
	}
	UImage* CrosshairImages[] = { Image_Center.Get(), Image_Top.Get(), Image_Bottom.Get(), Image_Left.Get(), Image_Right.Get() };
	for (UImage* ReticleImage : CrosshairImages)
	{
		if (ReticleImage)
		{
			ReticleImage->SetColorAndOpacity(Color);
		}
	}
}

void UShooterReticle::HandleViewportResized(FViewport* Viewport, uint32 Unused)
{
	ResetSpreadPresentation();
	RefreshSpreadOffsets();
}

void UShooterReticle::RefreshSpreadOffsets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UShooterReticle::RefreshSpreadOffsets);
	AWeaponBase* Weapon = FindEquippedWeapon();
	const bool bDynamicSpread = UsesDynamicSpread(Weapon);
	const float ComputedRadius = bDynamicSpread ? ProjectSpreadToRadius(Weapon->GetCurrentSpreadDegrees()) : 0.f;
	const float MinimumRadius = FMath::IsFinite(MinSpreadRadius) ? FMath::Max(0.f, MinSpreadRadius) : 0.f;
	const float Radius = FMath::IsFinite(ComputedRadius)
		? FMath::Max(ComputedRadius, MinimumRadius) : MinimumRadius;
	const bool bCanAnimate = bSpreadPresentationActive && bDynamicSpread
		&& FMath::IsFinite(SpreadReturnHalfLife) && SpreadReturnHalfLife > 0.f;
	const bool bReset = !bSpreadRadiusInitialized || SpreadPresentationWeapon.Get() != Weapon || !bCanAnimate;

	TargetSpreadRadius = Radius;
	SpreadPresentationWeapon = Weapon;
	bSpreadRadiusInitialized = true;
	if (bReset || Radius >= DisplayedSpreadRadius
		|| FMath::IsNearlyEqual(Radius, DisplayedSpreadRadius, SpreadSettleTolerance))
	{
		// 벌어질 때는 즉시 반영하고, 줄어드는 표시만 보간한다.
		StopSpreadAnimation();
		ApplySpreadRadius(Radius);
	}
	else
	{
		StartSpreadAnimation();
	}
}

void UShooterReticle::ApplySpreadRadius(const float Radius)
{
	DisplayedSpreadRadius = Radius;
	const float VerticalRadius = Radius * VerticalSpreadRadiusScale;
	SetSlotRenderOffset(Image_Right, HorizontalGroupOffset + FVector2D(Radius, 0.f));
	SetSlotRenderOffset(Image_Left, HorizontalGroupOffset + FVector2D(-Radius, 0.f));
	SetSlotRenderOffset(Image_Top, VerticalGroupOffset + FVector2D(0.f, -VerticalRadius - TopBarLift));
	SetSlotRenderOffset(Image_Bottom, VerticalGroupOffset + FVector2D(0.f, VerticalRadius));
}

void UShooterReticle::ResetSpreadPresentation()
{
	StopSpreadAnimation();
	bSpreadRadiusInitialized = false;
	SpreadPresentationWeapon.Reset();
	DisplayedSpreadRadius = TargetSpreadRadius = 0.f;
}

void UShooterReticle::StopSpreadAnimation()
{
	const TSharedPtr<SWidget> Widget = SpreadAnimationWidget.Pin();
	const TSharedPtr<FActiveTimerHandle> Timer = SpreadAnimationTimer.Pin();
	if (Widget.IsValid() && Timer.IsValid())
	{
		Widget->UnRegisterActiveTimer(Timer.ToSharedRef());
	}
	ClearSpreadAnimationHandles();
}

void UShooterReticle::ClearSpreadAnimationHandles()
{
	SpreadAnimationTimer.Reset();
	SpreadAnimationWidget.Reset();
	SpreadAnimationTime = 0.0;
}

void UShooterReticle::StartSpreadAnimation()
{
	if (!bSpreadPresentationActive || SpreadAnimationTimer.IsValid())
	{
		return;
	}
	if (const TSharedPtr<SWidget> Widget = GetCachedWidget(); Widget.IsValid() && FSlateApplication::IsInitialized())
	{
		SpreadAnimationWidget = Widget;
		SpreadAnimationTime = FSlateApplication::Get().GetCurrentTime();
		SpreadAnimationTimer = Widget->RegisterActiveTimer(0.f,
			FWidgetActiveTimerDelegate::CreateUObject(this, &ThisClass::AdvanceSpreadAnimation));
	}
	else
	{
		ApplySpreadRadius(TargetSpreadRadius);
	}
}

EActiveTimerReturnType UShooterReticle::AdvanceSpreadAnimation(const double CurrentTime, float)
{
	if (!bSpreadPresentationActive)
	{
		// 콜백 안에서는 직접 해제하지 않고 Stop을 반환한다.
		ClearSpreadAnimationHandles();
		return EActiveTimerReturnType::Stop;
	}
	const float Delta = static_cast<float>(FMath::Clamp(CurrentTime - SpreadAnimationTime, 0.0, MaxSpreadAnimationStep));
	SpreadAnimationTime = CurrentTime;
	const float RemainingFraction = SpreadReturnHalfLife > 0.f && FMath::IsFinite(SpreadReturnHalfLife)
		? FMath::Pow(0.5f, Delta / SpreadReturnHalfLife) : 0.f;
	const float NextRadius = TargetSpreadRadius + (DisplayedSpreadRadius - TargetSpreadRadius) * RemainingFraction;
	const bool bFinished = FMath::Abs(NextRadius - TargetSpreadRadius) <= SpreadSettleTolerance;
	ApplySpreadRadius(bFinished ? TargetSpreadRadius : NextRadius);
	if (bFinished)
	{
		// 콜백 안에서는 직접 해제하지 않고 Stop을 반환한다.
		ClearSpreadAnimationHandles();
		return EActiveTimerReturnType::Stop;
	}
	return EActiveTimerReturnType::Continue;
}

void UShooterReticle::NativeDestruct()
{
	bSpreadPresentationActive = false;
	ResetSpreadPresentation();
	// 델리게이트 해제
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->OnHitMarker.RemoveAll(this);
		PC->OnKillEffect.RemoveAll(this);
		PC->OnViewPresentationChanged.Remove(ViewPresentationHandle);
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	ViewPresentationHandle.Reset();
	UnbindFromCombatComponent();

	if (UShooterGameUserSettings* Settings = CachedCrosshairSettings.Get())
	{
		Settings->OnCrosshairSettingsChanged.Remove(CrosshairSettingsHandle);
	}
	CrosshairSettingsHandle.Reset();
	CachedCrosshairSettings.Reset();

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

void UShooterReticle::ReleaseSlateResources(const bool bReleaseChildren)
{
	bSpreadPresentationActive = false;
	ResetSpreadPresentation();
	Super::ReleaseSlateResources(bReleaseChildren);
}

float UShooterReticle::ComputeSpreadRadius() const
{
	return ProjectSpreadToRadius(GetSpreadDegrees());
}

float UShooterReticle::ProjectSpreadToRadius(const float SpreadDegrees) const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->PlayerCameraManager)
	{
		return 0.f;
	}

	if (!FMath::IsFinite(SpreadDegrees) || SpreadDegrees <= UE_KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	// 현재 총의 탄퍼짐 각도를 실제 화면상의 크로스헤어 간격으로 변환한다.
	// 화면 중심에서 그 점까지의 거리가 곧 크로스헤어 반경이 된다.
	const float SpreadRadiusRads = FMath::DegreesToRadians(SpreadDegrees);
	const float SpreadRadiusAtDistance = FMath::Tan(SpreadRadiusRads) * SpreadProjectionDistance;

	FVector CamPos;
	FRotator CamOrient;
	PC->PlayerCameraManager->GetCameraViewPoint(CamPos, CamOrient);

	const FVector CamForwDir = CamOrient.RotateVector(FVector::ForwardVector);
	const FVector CamUpDir = CamOrient.RotateVector(FVector::UpVector);
	const FVector OffsetTargetAtDistance = CamPos + (CamForwDir * SpreadProjectionDistance) + (CamUpDir * SpreadRadiusAtDistance);

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
	APawn* Pawn = GetViewedPawn();
	const AWeaponBase* Weapon = FindEquippedWeapon();
	if (!IsValid(Pawn) || !IsValid(Weapon))
	{
		return false;
	}

	const UCombatComponent* Combat = Pawn->Implements<UPlayerInterface>()
		? IPlayerInterface::Execute_GetCombatComponent(Pawn)
		: nullptr;
	return IsValid(Combat) && Combat->IsAiming() && Weapon->CanADS();
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

const UShooterGameUserSettings* UShooterReticle::GetCrosshairSettings() const
{
	const UShooterGameUserSettings* Settings = CachedCrosshairSettings.Get();
	return Settings ? Settings : UShooterGameUserSettings::Get();
}

bool UShooterReticle::UsesDynamicSpread(const AWeaponBase* Weapon) const
{
	const UShooterGameUserSettings* Settings = GetCrosshairSettings();
	return IsValid(Weapon) && Weapon->GetWeaponTag() != RogueTags::Weapon_Type_Sniper
		&& (!Settings || Settings->IsDynamicCrosshairEnabled());
}

float UShooterReticle::GetSpreadDegrees() const
{
	const AWeaponBase* Weapon = FindEquippedWeapon();
	// 정적/스나이퍼 예외는 표시만 고정하고 실제 탄퍼짐은 변경하지 않는다.
	return UsesDynamicSpread(Weapon) ? Weapon->GetCurrentSpreadDegrees() : 0.f;
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

APawn* UShooterReticle::GetViewedPawn() const
{
	// 관전 중에는 화면에 보이는 대상(뷰 타깃)의 무기 기준으로 스코프/크로스헤어를 그린다.
	// 킬캠 카메라처럼 Pawn이 아닌 뷰 타깃이면 내 Pawn으로 돌아간다.
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* ViewTargetPawn = Cast<APawn>(PC->GetViewTarget()))
		{
			return ViewTargetPawn;
		}
	}

	return GetOwningPlayerPawn();
}

AWeaponBase* UShooterReticle::FindEquippedWeapon() const
{
	APawn* Pawn = GetViewedPawn();
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
