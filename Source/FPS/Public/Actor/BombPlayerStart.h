#pragma once

#include "CoreMinimal.h"
#include "GameMode/RoundTypes.h"
#include "GameFramework/PlayerStart.h"
#include "BombPlayerStart.generated.h"

// Selects spawn locations by the current attack or defense role after side switching.
UCLASS(Blueprintable)
class FPS_API ABombPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Game Flow|Spawn")
	ETeamRole GetSpawnRole() const;

private:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Game Flow|Spawn", meta = (AllowPrivateAccess = "true"))
	ETeamRole SpawnRole = ETeamRole::None;
};
