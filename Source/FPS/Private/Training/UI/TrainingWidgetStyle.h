#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class UUserWidget;
class USoundBase;
class UShooterAugmentTierStyleSet;
enum class EAugmentTier : uint8;

// Styling helpers only. Widget hierarchy belongs to UMG/WBP so designers own layout.
namespace TrainingWidgetStyle
{
    FLinearColor GetAugmentTierColor(EAugmentTier Tier, const UShooterAugmentTierStyleSet* Palette);
    FText GetAugmentTierName(EAugmentTier Tier, const UShooterAugmentTierStyleSet* Palette);
    FSlateFontInfo MakeFont(int32 Size);
    bool RepairFont(FSlateFontInfo& Font);
    void ApplyButtonSounds(UUserWidget& Widget, USoundBase* HoverSound, USoundBase* ClickSound);
}
