#include "Customization/AppearanceParameters.h"
#include "Customization/CustomizationStudioActor.h"
#include "MuCO/CustomizableObjectInstance.h"

bool ApplyAppearanceParameters(UCustomizableObjectInstance* Instance,
    const UCustomizationStudioConfig* Config, const FCustomizationAppearance& Data)
{
    if (!Instance || !Config || !Data.ValidateForClass(Config->ClassId)) return false;
    Instance->SetIntParameterSelectedOption(TEXT("BodyType"), Data.BodyType);
    Instance->SetColorParameterSelectedOption(TEXT("EyeColor"), Data.EyeColor);
    Instance->SetColorParameterSelectedOption(TEXT("SkinColor"), Data.SkinColor);
    Instance->SetFloatParameterSelectedOption(TEXT("SkinValue"), Data.SkinValue);
    const bool AttachmentToggle = Config->ClassId == TEXT("Ranger") || Config->ClassId == TEXT("Vanguard");
    if (AttachmentToggle)
        Instance->SetIntParameterSelectedOption(TEXT("AttachmentVisibility"), Data.bAttachmentVisible ? TEXT("On") : TEXT("Off"));
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const FString& Part = FCustomizationAppearance::Parts()[Index];
        if (!Config->EnabledClothingParts.Contains(FName(*Part))) continue;
        Instance->SetColorParameterSelectedOption(Part + TEXT("Color"), Data.Colors[Index]);
        Instance->SetFloatParameterSelectedOption(Part + TEXT("Value"), Data.Values[Index]);
        Instance->SetFloatParameterSelectedOption(Part + TEXT("UseOriginal"), AttachmentToggle && Index == 3 ? 1.f : Data.Originals[Index]);
    }
    return true;
}
