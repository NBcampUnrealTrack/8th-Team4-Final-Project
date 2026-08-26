#pragma once

#include "CoreMinimal.h"
#include "Player/ShooterPlayerController.h"
#include "DominationPlayerController.generated.h"

// 라운드 입력과 UI에 의존하지 않는 Domination 전용 Controller 확장 지점이다.
UCLASS()
class FPS_API ADominationPlayerController : public AShooterPlayerController
{
	GENERATED_BODY()
};
