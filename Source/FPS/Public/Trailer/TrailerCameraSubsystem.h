// 트레일러 촬영용 카메라의 생성/해제를 관리한다. 개발/촬영 전용.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "TrailerCameraSubsystem.generated.h"

class ATrailerCameraActor;
class APlayerController;

UCLASS()
class FPS_API UTrailerCameraSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UTrailerCameraSubsystem* Get(const APlayerController* PlayerController);

	// 현재 시점에 카메라를 만들고 그쪽으로 시점을 옮긴다. 이미 있으면 시점만 다시 맞춘다.
	bool Activate(APlayerController* PlayerController);

	// 카메라를 지우고 시점을 원래대로 돌린다.
	void Deactivate(APlayerController* PlayerController);

	// 메인 HUD(RootLayout)와 화면 디버그 메시지를 숨기거나 되돌린다. 레이아웃은 그대로 두고 투명도만 바꿔서 입력/활성 상태에 영향이 없다.
	void SetHudHidden(bool bHidden);
	bool IsHudHidden() const { return bHudHidden; }

	bool IsActive() const { return Camera.IsValid(); }
	ATrailerCameraActor* GetCamera() const { return Camera.Get(); }

private:
	TWeakObjectPtr<ATrailerCameraActor> Camera;

	bool bHudHidden = false;
	// HUD를 숨기기 전의 화면 디버그 메시지 사용 여부(되돌릴 때 복원)
	bool bPreviousOnScreenMessages = true;
};
