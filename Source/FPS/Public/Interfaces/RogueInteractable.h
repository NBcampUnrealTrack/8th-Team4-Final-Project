#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RogueInteractable.generated.h"

class APlayerController;

// The supplied source has no general interaction interface. DataJack retains its existing GAS flow.
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URogueInteractable : public UInterface
{
    GENERATED_BODY()
};

class FPS_API IRogueInteractable
{
    GENERATED_BODY()

public:
    virtual bool CanInteract(const APlayerController& RequestingPlayer) const = 0;
    virtual void Interact(APlayerController& RequestingPlayer) = 0;
};
