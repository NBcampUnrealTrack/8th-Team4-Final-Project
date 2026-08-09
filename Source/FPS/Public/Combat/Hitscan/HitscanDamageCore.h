#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/HitscanTypes.h"

class FPS_API FHitscanDamageCore final
{
public:
	static FHitscanDamageCoreResult ResolveShot(const FHitscanDamageCoreRequest& Request);
};
