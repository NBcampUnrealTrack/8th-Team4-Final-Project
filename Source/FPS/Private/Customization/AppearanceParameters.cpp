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
    // Ranger's visible plate carrier and pouches are authored in the CO's Attachment
    // group; Vest only drives the shoulder straps. The Attachment button became the
    // sunglasses toggle, so the Vest selection colours both groups for Ranger.
    const bool bRangerVestDrivesAttachment = Config->ClassId == TEXT("Ranger");
    constexpr int32 VestIndex = 2;
    constexpr int32 AttachmentIndex = 3;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const FString& Part = FCustomizationAppearance::Parts()[Index];
        if (!Config->EnabledClothingParts.Contains(FName(*Part))) continue;
        const int32 Source = bRangerVestDrivesAttachment && Index == AttachmentIndex ? VestIndex : Index;
        Instance->SetColorParameterSelectedOption(Part + TEXT("Color"), Data.Colors[Source]);
        Instance->SetFloatParameterSelectedOption(Part + TEXT("Value"), Data.Values[Source]);
        const float UseOriginal = AttachmentToggle && Index == AttachmentIndex && !bRangerVestDrivesAttachment
            ? 1.f : Data.Originals[Source];
        Instance->SetFloatParameterSelectedOption(Part + TEXT("UseOriginal"), UseOriginal);
    }
    return true;
}
