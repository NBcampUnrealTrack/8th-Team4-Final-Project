#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class UWidgetTree;
class UUserWidget;
class USoundBase;
class UShooterAugmentTierStyleSet;
enum class EAugmentTier : uint8;

namespace TrainingWidgetLayout
{
    FLinearColor GetAugmentTierColor(EAugmentTier Tier, const UShooterAugmentTierStyleSet* Palette);
    FText GetAugmentTierName(EAugmentTier Tier, const UShooterAugmentTierStyleSet* Palette);
    FSlateFontInfo MakeFont(int32 Size);
    bool RepairFont(FSlateFontInfo& Font);
    void RepairTextFonts(UWidgetTree& Tree);

    void BuildSessionHUD(UWidgetTree& Tree);
    void BuildResults(UWidgetTree& Tree);
    void BuildCountdown(UWidgetTree& Tree);
    void ApplyButtonSounds(UUserWidget& Widget, USoundBase* HoverSound, USoundBase* ClickSound);
}
