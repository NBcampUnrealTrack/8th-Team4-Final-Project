#pragma once
#include "Customization/CustomizationProfile.h"

// Slate may evaluate a value attribute even when its parent is collapsed.
inline float MutableBrightness(const FCustomizationAppearance& Draft, int32 Selected)
{
    if (Selected == 8) return (Draft.SkinValue - .15f) / 1.2f;
    return Selected >= 0 && Selected < 8 && Draft.Values.IsValidIndex(Selected)
        ? Draft.Values[Selected] / 2.f : 0.f;
}
