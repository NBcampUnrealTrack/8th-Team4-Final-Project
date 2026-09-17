#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
#include "UObject/Interface.h"
#include "RogueCombatClassSelector.generated.h"

// Shared class-selection UI commands. Each controller retains its own authoritative mode rules.
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URogueCombatClassSelector : public UInterface
{
    GENERATED_BODY()
};

class FPS_API IRogueCombatClassSelector
{
    GENERATED_BODY()

public:
    virtual void SelectCombatClass(EDominationCombatClass RequestedClass) = 0;
    virtual void SetPreviewCombatClass(EDominationCombatClass RequestedClass) = 0;
    // Negative means there is no selection deadline, not a zero-second countdown.
    virtual float GetCombatClassSelectionTimeRemaining() const = 0;
};
