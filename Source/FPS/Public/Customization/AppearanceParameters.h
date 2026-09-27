#pragma once
#include "CoreMinimal.h"
#include "Customization/CustomizationProfile.h"
class UCustomizableObjectInstance;
class UCustomizationStudioConfig;

FPS_API bool ApplyAppearanceParameters(UCustomizableObjectInstance* Instance,
    const UCustomizationStudioConfig* Config, const FCustomizationAppearance& Data);
