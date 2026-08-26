#pragma once

#include "CoreMinimal.h"
#include "GameMode/BaseTeamGameMode.h"
#include "DominationGameMode.generated.h"

class APlayerController;

// Domination의 전용 타입 구성과 초기 진입 경계를 담당한다.
UCLASS()
class FPS_API ADominationGameMode : public ABaseTeamGameMode
{
	GENERATED_BODY()

public:
	ADominationGameMode();

	virtual void NotifyPlayerReadyForGameFlow(APlayerController* PlayerController) override;
};
