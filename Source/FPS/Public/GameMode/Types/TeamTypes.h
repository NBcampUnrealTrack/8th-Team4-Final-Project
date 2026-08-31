#pragma once

#include "CoreMinimal.h"
#include "TeamTypes.generated.h"

// Shared team identifiers keep objective-specific rules out of the common team framework.
UENUM(BlueprintType)
enum class EGameTeam : uint8
{
	None UMETA(DisplayName = "None"),
	TeamA UMETA(DisplayName = "Team A"),
	TeamB UMETA(DisplayName = "Team B")
};
