#pragma once

#include "CoreMinimal.h"
#include "Augment/AugmentTypes.h"
#include "GameMode/Types/DominationTypes.h"
#include "TrainingLoadoutSnapshot.generated.h"

USTRUCT()
struct FPS_API FTrainingLoadoutSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    EDominationCombatClass CombatClass = EDominationCombatClass::Unselected;

    UPROPERTY()
    TArray<FAugmentInventoryEntry> Augments;
};
