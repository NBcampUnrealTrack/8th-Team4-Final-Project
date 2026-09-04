#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "DataJackSpawnPoint.generated.h"

// Place exactly one instance at the fixed attacker-side objective spawn.
UCLASS(Blueprintable)
class FPS_API ADataJackSpawnPoint : public ATargetPoint
{
	GENERATED_BODY()
};
