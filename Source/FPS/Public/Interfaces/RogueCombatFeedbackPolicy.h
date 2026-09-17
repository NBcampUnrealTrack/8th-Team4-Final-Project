#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RogueCombatFeedbackPolicy.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URogueCombatFeedbackPolicy : public UInterface
{
    GENERATED_BODY()
};

class FPS_API IRogueCombatFeedbackPolicy
{
    GENERATED_BODY()

public:
    // False keeps target destruction out of kill/death gameplay events and kill-marker treatment.
    virtual bool CountsAsCombatKill() const { return true; }

    // False keeps visual hit markers but suppresses the generic hit/kill feedback audio.
    virtual bool PlaysCombatFeedbackAudio() const { return true; }
};
