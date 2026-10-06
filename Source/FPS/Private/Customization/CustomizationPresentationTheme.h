#pragma once

#include "CoreMinimal.h"

// Shared by the inspector and its color dialog. Values are linear Slate colors.
namespace CustomizationPresentationTheme
{
inline const FLinearColor Accent = FLinearColor::FromSRGBColor(FColor(213, 173, 43));
inline const FLinearColor Text = FLinearColor::FromSRGBColor(FColor(235, 233, 224));
inline const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor(169, 169, 157));
inline const FLinearColor Panel = FLinearColor::FromSRGBColor(FColor(26, 29, 29, 248));
inline const FLinearColor Button = FLinearColor::FromSRGBColor(FColor(47, 51, 50));
inline const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(20, 22, 21));
}
