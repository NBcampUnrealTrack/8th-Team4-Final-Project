// ShooterOutGameTypes.h
#pragma once

#include "CoreMinimal.h"
#include "ShooterOutGameTypes.generated.h"

UENUM(BlueprintType)
enum class EShooterOutGameMusicState : uint8
{
	None,
	Frontend,
	Room
};
