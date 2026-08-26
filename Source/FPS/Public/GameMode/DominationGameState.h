#pragma once

#include "CoreMinimal.h"
#include "GameMode/BaseTeamMatchGameState.h"
#include "DominationGameState.generated.h"

// Domination 전용 상태는 이후 매치 흐름과 점령 진행 커밋에서 확장한다.
UCLASS()
class FPS_API ADominationGameState : public ABaseTeamMatchGameState
{
	GENERATED_BODY()
};
