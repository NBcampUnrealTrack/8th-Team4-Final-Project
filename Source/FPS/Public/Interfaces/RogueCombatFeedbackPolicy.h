#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RogueCombatFeedbackPolicy.generated.h"

class AActor;

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

    // False keeps gameplay kill semantics intact, but opts out of competitive kill/death stats,
    // standard elimination feed, and multi-kill presentation.
    virtual bool UsesStandardEliminationFeedback() const { return true; }
};

namespace RogueCombatFeedbackPolicy
{
    // Resolves a policy from the actor, its components, and for Pawns the owning Controller.
    FPS_API const IRogueCombatFeedbackPolicy* Resolve(const AActor* Actor);
}
