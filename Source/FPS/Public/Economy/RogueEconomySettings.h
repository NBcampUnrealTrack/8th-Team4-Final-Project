#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "RogueEconomySettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Credit Economy"))
class FPS_API URogueEconomySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Credits", meta = (ClampMin = "0", UIMin = "0"))
	int32 StartingCredits = 1000;

	UPROPERTY(Config, EditAnywhere, Category = "Credits", meta = (ClampMin = "0", UIMin = "0"))
	int32 RoundWinReward = 1000;

	UPROPERTY(Config, EditAnywhere, Category = "Credits", meta = (ClampMin = "0", UIMin = "0"))
	int32 RoundLossReward = 600;

	UPROPERTY(Config, EditAnywhere, Category = "Credits|Performance", meta = (ClampMin = "0", UIMin = "0"))
	int32 KillReward = 100;

	// 설치 또는 해체를 완료한 팀의 라운드 참가자 "한 명당" 지급액이다. 팀 총액이 아니다.
	// 라운드 승패와 무관하게 지급되므로, 설치 후 해체당한 라운드에서는 양 팀이 모두 받는다.
	UPROPERTY(Config, EditAnywhere, Category = "Credits|Performance", meta = (ClampMin = "0", UIMin = "0"))
	int32 TeamObjectiveRewardPerPlayer = 300;
};
