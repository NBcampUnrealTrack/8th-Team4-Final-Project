#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"

class UAugmentDefinition;
class AShooterPlayerState;

namespace TrainingAugmentRules
{
    inline constexpr int32 MaximumSelection = 3;
    enum class EToggleResult : uint8 { Selected, Deselected, LimitReached, InvalidId };

    FPS_API bool GetAvailabilityTags(const AShooterPlayerState* PlayerState,
        FGameplayTagContainer& OutTags);
    FPS_API bool IsCatalogEligible(const UAugmentDefinition& Definition,
        const FGameplayTagContainer& UnlockTags);
    FPS_API EToggleResult Toggle(TArray<FPrimaryAssetId>& Selection, const FPrimaryAssetId& Id);
    FPS_API bool ValidateIds(const TArray<FPrimaryAssetId>& Selection, FText& OutFailure);
    FPS_API FGameplayTag GetActiveSlot(int32 ActiveIndex);
    FPS_API FText GetSlotLabel(int32 ActiveIndex);
}
