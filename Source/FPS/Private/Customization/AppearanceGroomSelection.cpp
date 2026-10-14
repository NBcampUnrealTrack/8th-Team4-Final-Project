#include "Customization/AppearanceGroomSelection.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationClassRules.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"

bool AppearanceGroomSelection::Select(const UCustomizationStudioConfig& Config, const FString& BodyType,
    FName HairId, TArray<FStudioGroomChoice>& OutChoices, FString& OutError)
{
    OutChoices.Reset(); OutError.Reset();
    if (BodyType != FixedCustomizationBodyForClass(Config.ClassId) || !Config.BodyTypes.Contains(BodyType))
    { OutError = TEXT("Unknown groom body type"); return false; }
    bool bFoundHair = HairId == TEXT("Bald");
    for (FName Part : {FName(TEXT("Hair")), FName(TEXT("Eyebrows")), FName(TEXT("Eyelashes")),
        FName(TEXT("Fuzz")), FName(TEXT("Mustache")), FName(TEXT("Beard"))})
    {
        if (Part == TEXT("Hair") && HairId == TEXT("Bald")) continue;
        const auto* Choice = Config.Grooms.FindByPredicate([&](const FStudioGroomChoice& H)
        { return H.BodyType == BodyType && H.Component == Part && (Part != TEXT("Hair") || H.Id == HairId); });
        if (!Choice) continue; // A missing optional facial-hair part stays absent.
        if (Part == TEXT("Hair") && !Choice->bSelectable)
        { OutChoices.Reset(); OutError = TEXT("Hairstyle is not available for this body"); return false; }
        if (Choice->Groom.IsNull() || Choice->Binding.IsNull())
        { OutChoices.Reset(); OutError = TEXT("Selected groom or binding path missing"); return false; }
        OutChoices.Add(*Choice);
        if (Part == TEXT("Hair")) bFoundHair = true;
    }
    if (!bFoundHair) { OutChoices.Reset(); OutError = TEXT("Unknown hairstyle"); return false; }
    return true;
}

void AppearanceGroomSelection::GatherPaths(const TArray<FStudioGroomChoice>& Choices, TArray<FSoftObjectPath>& OutPaths)
{
    OutPaths.Reset();
    for (const auto& Choice : Choices)
    {
        OutPaths.AddUnique(Choice.Groom.ToSoftObjectPath());
        OutPaths.AddUnique(Choice.Binding.ToSoftObjectPath());
    }
}

bool AppearanceGroomSelection::ValidateLoaded(const TArray<FStudioGroomChoice>& Choices,
    USkeletalMesh* BindingSurface, FString& OutError)
{
    OutError.Reset();
    for (const auto& Choice : Choices)
    {
        auto* Groom = Choice.Groom.Get(); auto* Binding = Choice.Binding.Get();
        if (!Groom || !Binding || !BindingSurface
            || !UGroomBindingAsset::IsCompatible(Groom, Binding, false)
            || !UGroomBindingAsset::IsCompatible(BindingSurface, Binding, false))
        { OutError = TEXT("Selected groom failed to load or is incompatible with the binding face"); return false; }
    }
    return true;
}
