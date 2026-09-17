#pragma once

#include "CoreMinimal.h"
#include "Augment/AugmentTypes.h"
#include "GameMode/Types/DominationTypes.h"
#include "GameplayTagContainer.h"
#include "TrainingLoadoutSnapshot.generated.h"

// 클래스·무기·증강의 서로 다른 복제 순서에 UI가 의존하지 않도록 한 응답에 묶는다.
USTRUCT()
struct FPS_API FTrainingLoadoutSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    EDominationCombatClass CombatClass = EDominationCombatClass::Unselected;

    UPROPERTY()
    TArray<FAugmentInventoryEntry> Augments;

    UPROPERTY()
    FGameplayTagContainer AvailabilityTags;

    UPROPERTY()
    bool bAvailabilityReady = false;
};
