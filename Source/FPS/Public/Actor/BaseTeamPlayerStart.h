#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/TeamTypes.h"
#include "GameFramework/PlayerStart.h"
#include "BaseTeamPlayerStart.generated.h"

// Team-based modes can reuse this spawn point without depending on objective-specific rules.
UCLASS(Blueprintable)
class FPS_API ABaseTeamPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Game Flow|Spawn")
	EGameTeam GetSpawnTeam() const;

private:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Game Flow|Spawn", meta = (AllowPrivateAccess = "true"))
	EGameTeam SpawnTeam = EGameTeam::None;
};
