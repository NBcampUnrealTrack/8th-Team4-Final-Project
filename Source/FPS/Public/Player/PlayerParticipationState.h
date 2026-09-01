#pragma once

#include "CoreMinimal.h"
#include "PlayerParticipationState.generated.h"

UENUM(BlueprintType)
enum class EPlayerParticipationState : uint8
{
	WaitingForMatchStart UMETA(DisplayName = "Waiting For Match Start"),
	Active UMETA(DisplayName = "Active"),
	WaitingForNextSpawn UMETA(DisplayName = "Waiting For Next Spawn")
};
