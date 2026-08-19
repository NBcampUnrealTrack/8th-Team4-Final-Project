#pragma once

#include "CoreMinimal.h"
#include "Player/BaseRoundPlayerController.h"
#include "BombPlayerController.generated.h"

/**
 * 폭탄 모드용 PlayerController 타입.
 * Objective 입력은 공통 InputTag -> ASC -> GameplayAbility 경로에서 처리한다.
 */
UCLASS()
class FPS_API ABombPlayerController : public ABaseRoundPlayerController
{
	GENERATED_BODY()
};
