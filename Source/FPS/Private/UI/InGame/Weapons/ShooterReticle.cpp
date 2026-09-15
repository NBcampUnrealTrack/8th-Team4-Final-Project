// ShooterReticle.cpp
#include "UI/InGame/Weapons/ShooterReticle.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
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

// Offset 벡터를 AngleDegrees만큼 회전시킨다(2D 회전 변환).
static FVector2D RotateOffset(const FVector2D& Offset, const float AngleDegrees)
{
	const float Rad = FMath::DegreesToRadians(AngleDegrees);
	const float CosA = FMath::Cos(Rad);
	const float SinA = FMath::Sin(Rad);
	return FVector2D(
		Offset.X * CosA - Offset.Y * SinA,
		Offset.X * SinA + Offset.Y * CosA);
}

// Right 이미지 하나만 WBP에서 브러시/크기를 잡아두면, 나머지 3방향은 Right를 그대로 복사한 뒤
// Position/Angle을 Right 기준 벡터를 회전시켜서 재사용한다. 디자이너에서 4개를 따로 맞출 필요가 없어진다.
static void SyncDirectionalReticleImage(const UImage* Source, UImage* Target, const float AngleDegrees)
{
	if (!Source || !Target)
	{
		return;
	}

	Target->SetBrush(Source->GetBrush());

	const UCanvasPanelSlot* SourceSlot = Cast<UCanvasPanelSlot>(Source->Slot);
	UCanvasPanelSlot* TargetSlot = Cast<UCanvasPanelSlot>(Target->Slot);
	if (SourceSlot && TargetSlot)
	{
		TargetSlot->SetSize(SourceSlot->GetSize());

		// Source의 실제 Position 벡터를 그대로 회전시키지 않는다 - Source 자체가 완벽한 수평/수직이
		// 아니면 그 오차가 4방향 모두에 전파된다. 대신 거리(반경)만 뽑아서 정확히 0/90/180/270도
		// 방향으로 다시 배치한다.
		const float SourceRadius = SourceSlot->GetPosition().Size();
		TargetSlot->SetPosition(RotateOffset(FVector2D(SourceRadius, 0.f), AngleDegrees));
	}

	// 각 이미지가 자기 자신의 중심을 기준으로 회전하도록 Pivot을 강제로 정중앙에 고정한다.
	// (WBP에서 예전에 만져둔 Pivot 값이 남아있으면 엉뚱한 지점을 기준으로 돌아간다.)
	Target->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Target->SetRenderTransformAngle(AngleDegrees);
}

void UShooterReticle::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ConfigureCenteredCanvasSlot(Image_Top);
	ConfigureCenteredCanvasSlot(Image_Bottom);
	ConfigureCenteredCanvasSlot(Image_Left);
	ConfigureCenteredCanvasSlot(Image_Right);
	ConfigureCenteredCanvasSlot(Image_Center);

	// ==================== [CROSSHAIRDEBUG] 하드코딩 최소 테스트 (진단 끝나면 제거) ====================
	// 계산/회전 로직을 전부 배제하고, 4개 이미지에 브러시/앵커/얼라인먼트/피벗/각도/포지션을
	// 숫자 그대로 박아 넣는다. 이래도 정십자가 안 나오면 문제는 Anchor/Alignment나 부모 CanvasPanel
	// 기준점 쪽이다.
	{
		const FSlateBrush RightBrush = Image_Right ? Image_Right->GetBrush() : FSlateBrush();
		const UCanvasPanelSlot* RightSlot = Image_Right ? Cast<UCanvasPanelSlot>(Image_Right->Slot) : nullptr;
		const FVector2D RightSize = RightSlot ? RightSlot->GetSize() : FVector2D(30.f, 30.f);
		UE_LOG(LogTemp, Warning, TEXT("[CROSSHAIRDEBUG] Right Size=(%.3f,%.3f) - 이 값을 4개 다 강제로 통일한다"), RightSize.X, RightSize.Y);

		auto ApplyHardcodedCross = [&RightBrush, &RightSize](UImage* Img, const float X, const float Y, const float AngleDegrees)
		{
			if (!Img)
			{
				return;
			}
			Img->SetBrush(RightBrush);
			Img->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Img->SetRenderTransformAngle(AngleDegrees);
			if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Img->Slot))
			{
				Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				Slot->SetAlignment(FVector2D(0.5f, 0.5f));
				Slot->SetSize(RightSize);
				Slot->SetPosition(FVector2D(X, Y));
			}
		};
		ApplyHardcodedCross(Image_Right,  30.f,   0.f,   0.f);
		ApplyHardcodedCross(Image_Left,  -30.f,   0.f, 180.f);
		ApplyHardcodedCross(Image_Top,     0.f, -30.f,  90.f);
		ApplyHardcodedCross(Image_Bottom,  0.f,  30.f, 270.f);
	}
	// SyncDirectionalReticleImage(Image_Right, Image_Left, 180.f);
	// SyncDirectionalReticleImage(Image_Right, Image_Top, 270.f);
	// SyncDirectionalReticleImage(Image_Right, Image_Bottom, 90.f);
	// ==================== END [CROSSHAIRDEBUG] 하드코딩 테스트 ====================

	SetSlotPosition(Image_Center, FVector2D::ZeroVector);

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

void UShooterReticle::NativeDestruct()
{
	// 델리게이트 해제
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->OnHitMarker.RemoveAll(this);
		PC->OnKillEffect.RemoveAll(this);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitMarkerTimerHandle);
		World->GetTimerManager().ClearTimer(KillEffectTimerHandle);
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

bool UShooterReticle::IsHitMarkerActive() const
{
	return bHitMarkerActive;
}

void UShooterReticle::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// [CROSSHAIRDEBUG] 하드코딩 테스트 중에는 매 프레임 Position을 덮어쓰지 않도록 잠시 꺼둔다.
	// 진단 끝나면 아래 블록을 원복한다.
	// const float Radius = FMath::Max(ComputeSpreadRadius(), MinSpreadRadius);
	// const FVector2D RightOffset(Radius, 0.f);
	// SetSlotPosition(Image_Right,  RightOffset);
	// SetSlotPosition(Image_Left,   RotateOffset(RightOffset, 180.f));
	// SetSlotPosition(Image_Top,    RotateOffset(RightOffset, 270.f));
	// SetSlotPosition(Image_Bottom, RotateOffset(RightOffset, 90.f));
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

