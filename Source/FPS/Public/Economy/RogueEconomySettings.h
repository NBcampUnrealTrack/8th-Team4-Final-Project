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

	// 목표 달성 팀의 라운드 참가자 한 명당 지급되는 보상이다.
	UPROPERTY(Config, EditAnywhere, Category = "Credits|Performance", meta = (ClampMin = "0", UIMin = "0"))
	int32 TeamObjectiveRewardPerPlayer = 300;
};