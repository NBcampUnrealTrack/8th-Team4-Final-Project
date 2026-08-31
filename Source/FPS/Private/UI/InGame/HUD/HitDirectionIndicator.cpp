// HitDirectionIndicator.cpp
#include "UI/InGame/HUD/HitDirectionIndicator.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "GAS/RogueHealthComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "TimerManager.h"

void UHitDirectionIndicator::NativeConstruct()
{
	Super::NativeConstruct();

	TryBindHealthComponent();
}

void UHitDirectionIndicator::NativeDestruct()
{
	UnbindHealthComponent();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthBindingRetryTimerHandle);
	}

	Super::NativeDestruct();
}

void UHitDirectionIndicator::TryBindHealthComponent()
{
	AShooterPlayerState* OwningPlayerState = GetOwningPlayerState<AShooterPlayerState>();
	URogueHealthComponent* HealthComponent = OwningPlayerState ? OwningPlayerState->GetHealthComponent() : nullptr;

	if (!IsValid(HealthComponent))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				HealthBindingRetryTimerHandle,
				this,
				&ThisClass::TryBindHealthComponent,
				0.1f,
				false);
		}
		return;
	}

	if (BoundHealthComponent.Get() != HealthComponent)
	{
		UnbindHealthComponent();
		BoundHealthComponent = HealthComponent;
		HealthComponent->OnHitReactionReceived.AddUniqueDynamic(this, &ThisClass::HandleHitReactionReceived);
		UE_LOG(LogTemp, Warning, TEXT("[HitDirectionIndicator] %s: HealthComponent 바인딩 완료."), *GetName());
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthBindingRetryTimerHandle);
	}
}

void UHitDirectionIndicator::UnbindHealthComponent()
{
	if (URogueHealthComponent* HealthComponent = BoundHealthComponent.Get())
	{
		HealthComponent->OnHitReactionReceived.RemoveDynamic(this, &ThisClass::HandleHitReactionReceived);
	}
	BoundHealthComponent.Reset();
}

void UHitDirectionIndicator::HandleHitReactionReceived(FVector WorldDirection)
{
	UE_LOG(LogTemp, Warning, TEXT("[HitDirectionIndicator] %s: 이벤트 수신. Image_HitDirection=%s Anim_HitFade=%s"),
		*GetName(),
		Image_HitDirection ? TEXT("있음") : TEXT("None"),
		Anim_HitFade ? TEXT("있음") : TEXT("None"));

	if (!Image_HitDirection || WorldDirection.IsNearlyZero())
	{
		return;
	}

	const APlayerController* OwningController = GetOwningPlayer();
	float CameraYaw = 0.f;
	if (OwningController)
	{
		CameraYaw = OwningController->PlayerCameraManager
			? OwningController->PlayerCameraManager->GetCameraRotation().Yaw
			: OwningController->GetControlRotation().Yaw;
	}

	const float DirectionYaw = WorldDirection.Rotation().Yaw;
	const float RelativeAngleDegrees = FMath::FindDeltaAngleDegrees(CameraYaw, DirectionYaw);

	// 아트 에셋은 "정면 위쪽"을 가리키는 형태로 제작한다 - 0도일 때 화면 위쪽 = 정면에서 맞았다는 뜻.
	Image_HitDirection->SetRenderTransformAngle(RelativeAngleDegrees);

	if (Anim_HitFade)
	{
		PlayAnimation(Anim_HitFade, 0.f);
	}
}
