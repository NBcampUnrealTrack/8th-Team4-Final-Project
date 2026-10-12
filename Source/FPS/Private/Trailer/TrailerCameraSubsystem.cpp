#include "Trailer/TrailerCameraSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Trailer/TrailerCameraActor.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

UTrailerCameraSubsystem* UTrailerCameraSubsystem::Get(const APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = IsValid(PlayerController) ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UTrailerCameraSubsystem>() : nullptr;
}

bool UTrailerCameraSubsystem::Activate(APlayerController* PlayerController)
{
	UWorld* World = IsValid(PlayerController) ? PlayerController->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	if (!Camera.IsValid())
	{
		// 현재 보고 있는 위치와 방향에서 시작한다.
		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;

		Camera = World->SpawnActor<ATrailerCameraActor>(ATrailerCameraActor::StaticClass(), ViewLocation, ViewRotation, SpawnParameters);
	}

	if (!Camera.IsValid())
	{
		return false;
	}

	PlayerController->SetViewTargetWithBlend(Camera.Get(), 0.5f);
	return true;
}

void UTrailerCameraSubsystem::Deactivate(APlayerController* PlayerController)
{
	// 카메라를 끌 때 HUD와 디버그 메시지도 원래대로 돌려 놓는다.
	SetHudHidden(false);

	if (Camera.IsValid())
	{
		Camera->Destroy();
	}
	Camera.Reset();

	if (IsValid(PlayerController))
	{
		PlayerController->SetViewTargetWithBlend(PlayerController->GetPawn(), 0.5f);
	}
}

void UTrailerCameraSubsystem::SetHudHidden(const bool bHidden)
{
	if (bHudHidden == bHidden)
	{
		return;
	}
	bHudHidden = bHidden;

	// 메인 HUD는 RootLayout 아래에 모두 올라가 있으므로 투명도만 바꿔 한 번에 숨긴다.
	// Visibility를 바꾸면 CommonUI의 입력 설정/포커스 계산에 영향을 줄 수 있어 쓰지 않는다.
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UGameInstance* GameInstance = LocalPlayer->GetGameInstance())
		{
			if (UShooterUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UShooterUIManagerSubsystem>())
			{
				if (UShooterPrimaryGameLayout* RootLayout = UIManager->GetRootLayout())
				{
					RootLayout->SetRenderOpacity(bHidden ? 0.f : 1.f);
				}
			}
		}
	}

	// 화면 디버그 메시지(핑, 네트워크 경고 등)도 녹화에 찍히지 않도록 함께 끈다.
	if (GEngine)
	{
		if (bHidden)
		{
			bPreviousOnScreenMessages = GEngine->bEnableOnScreenDebugMessages;
			GEngine->bEnableOnScreenDebugMessages = false;
		}
		else
		{
			GEngine->bEnableOnScreenDebugMessages = bPreviousOnScreenMessages;
		}
	}
}
