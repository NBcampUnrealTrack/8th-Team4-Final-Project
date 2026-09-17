#include "TrainingWidgetStyle.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/Font.h"
#include "Sound/SlateSound.h"
#include "UI/Common/ShooterAugmentTierStyleSet.h"
#include "UObject/UObjectGlobals.h"

namespace TrainingWidgetStyle
{
FLinearColor GetAugmentTierColor(const EAugmentTier Tier, const UShooterAugmentTierStyleSet* Palette)
{
    if (const FShooterAugmentTierStyle* Style = Palette ? Palette->FindStyle(Tier) : nullptr)
    {
        FLinearColor Color = Style->AccentColor;
        Color.A = 1.0f;
        return Color;
    }
    switch (Tier)
    {
    case EAugmentTier::Rare: return FLinearColor(FColor(80, 167, 255));
    case EAugmentTier::Epic: return FLinearColor(FColor(191, 125, 255));
    case EAugmentTier::Legendary: return FLinearColor(FColor(255, 197, 80));
    default: return FLinearColor(FColor(168, 183, 197));
    }
}

FText GetAugmentTierName(const EAugmentTier Tier, const UShooterAugmentTierStyleSet* Palette)
{
    if (const FShooterAugmentTierStyle* Style = Palette ? Palette->FindStyle(Tier) : nullptr)
    {
        if (!Style->TierDisplayName.IsEmpty()) { return Style->TierDisplayName; }
    }
    switch (Tier)
    {
    case EAugmentTier::Rare: return NSLOCTEXT("TrainingAugmentStyle", "Rare", "RARE");
    case EAugmentTier::Epic: return NSLOCTEXT("TrainingAugmentStyle", "Epic", "EPIC");
    case EAugmentTier::Legendary: return NSLOCTEXT("TrainingAugmentStyle", "Legendary", "LEGENDARY");
    default: return NSLOCTEXT("TrainingAugmentStyle", "Common", "COMMON");
    }
}

namespace
{
    UFont* DefaultFontAsset()
    {
        static TWeakObjectPtr<UFont> CachedFont;
        if (!CachedFont.IsValid())
        {
            CachedFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
            ensureMsgf(CachedFont.IsValid(), TEXT("[TrainingUI] Cannot load /Engine/EngineFonts/Roboto.Roboto"));
        }
        return CachedFont.Get();
    }

    bool HasPersistentFont(const FSlateFontInfo& Font)
    {
        const UFont* Asset = Cast<UFont>(Font.FontObject.Get());
        return Asset && !Asset->HasAnyFlags(RF_Transient)
            && Asset->FontCacheType == EFontCacheType::Runtime;
    }
}

FSlateFontInfo MakeFont(const int32 Size)
{
    return FSlateFontInfo(DefaultFontAsset(), Size, FName(TEXT("Bold")));
}

bool RepairFont(FSlateFontInfo& Font)
{
    if (HasPersistentFont(Font)) { return false; }
    UFont* DefaultFont = DefaultFontAsset();
    if (!DefaultFont) { return false; }
    Font.FontObject = DefaultFont;
    Font.CompositeFont.Reset();
    Font.TypefaceFontName = FName(TEXT("Bold"));
    return true;
}

void ApplyButtonSounds(UUserWidget& Widget, USoundBase* HoverSound, USoundBase* ClickSound)
{
    if (!Widget.WidgetTree || (!HoverSound && !ClickSound)) { return; }
    Widget.WidgetTree->ForEachWidget([HoverSound, ClickSound](UWidget* Candidate)
    {
        if (UButton* Button = Cast<UButton>(Candidate))
        {
            FButtonStyle Style = Button->GetStyle();
            if (HoverSound)
            {
                FSlateSound Hover;
                Hover.SetResourceObject(HoverSound);
                Style.SetHoveredSound(Hover);
            }
            if (ClickSound)
            {
                FSlateSound Click;
                Click.SetResourceObject(ClickSound);
                Style.SetPressedSound(Click);
            }
            Button->SetStyle(Style);
        }
        if (UUserWidget* Nested = Cast<UUserWidget>(Candidate))
        {
            ApplyButtonSounds(*Nested, HoverSound, ClickSound);
        }
    });
}
}
