#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RogueHitscanReactiveTarget.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URogueHitscanReactiveTarget : public UInterface
{
    GENERATED_BODY()
};

class FPS_API IRogueHitscanReactiveTarget
{
    GENERATED_BODY()

public:
    virtual void HandleHitscanImpact(AActor* InstigatorActor, const FVector& ImpactPoint,
        const FVector& ShotDirection) = 0;
};
