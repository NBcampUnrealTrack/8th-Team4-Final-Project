#include "Training/UI/TrainingAugmentTileWidget.h"

#include "Augment/AugmentDefinition.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Training/TrainingAugmentRules.h"
#include "UI/Common/ShooterAugmentTierStyleSet.h"

#define LOCTEXT_NAMESPACE "TrainingAugmentTile"

namespace TrainingAugmentTileStyle
{
    FLinearColor FallbackAccent(const EAugmentTier Tier)
    {
        switch (Tier)
        {
        case EAugmentTier::Rare:      return FLinearColor(FColor(80, 167, 255));
        case EAugmentTier::Epic:      return FLinearColor(FColor(191, 125, 255));
        case EAugmentTier::Legendary: return FLinearColor(FColor(255, 197, 80));
        default:                     return FLinearColor(FColor(168, 183, 197));
        }
    }

    FLinearColor ResolveAccent(const EAugmentTier Tier, const UShooterAugmentTierStyleSet* Palette)
    {
        if (!Palette) { return FallbackAccent(Tier); }
        FLinearColor Colors[static_cast<int32>(EAugmentTier::MAX)];
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Colors); ++Index)
        {
            const EAugmentTier CurrentTier = static_cast<EAugmentTier>(Index);
            const FShooterAugmentTierStyle* Style = Palette->FindStyle(CurrentTier);
            Colors[Index] = Style ? Style->AccentColor : FallbackAccent(CurrentTier);
            Colors[Index].A = 1.0f;
            for (int32 Previous = 0; Previous < Index; ++Previous)
            {
                if (Colors[Index].Equals(Colors[Previous])) { return FallbackAccent(Tier); }
            }
        }
        return Tier < EAugmentTier::MAX ? Colors[static_cast<int32>(Tier)] : FallbackAccent(Tier);
    }

    void TintBrush(FSlateBrush& Brush, const FLinearColor& Fill, const FLinearColor& Edge, const float Width)
    {
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.SetResourceObject(nullptr);
        Brush.TintColor = FSlateColor(Fill);
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        Brush.OutlineSettings.CornerRadii = FVector4(4.0f, 4.0f, 4.0f, 4.0f);
        Brush.OutlineSettings.Color = FSlateColor(Edge);
        Brush.OutlineSettings.Width = Width;
    }

    FButtonStyle ApplyTier(FButtonStyle Style, FLinearColor Accent, const bool bSelected)
    {
        Accent.A = 1.0f;
        const FLinearColor Base(FColor(15, 23, 31));
        TintBrush(Style.Normal, FMath::Lerp(Base, Accent, bSelected ? 0.16f : 0.065f),
            FMath::Lerp(Base, Accent, bSelected ? 1.0f : 0.55f), bSelected ? 2.5f : 1.25f);
        TintBrush(Style.Hovered, FMath::Lerp(Base, Accent, bSelected ? 0.22f : 0.13f), Accent, 2.0f);
        TintBrush(Style.Pressed, FMath::Lerp(Base, Accent, 0.25f), Accent, 2.5f);
        Style.Disabled = Style.Normal;
        Style.NormalPadding = FMargin(14.0f, 12.0f);
        Style.PressedPadding = Style.NormalPadding;
        return Style;
    }
}

void UTrainingAugmentTileWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (Btn_Toggle) { Btn_Toggle->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked); }
}

void UTrainingAugmentTileWidget::SetDefinition(UAugmentDefinition& Definition)
{
    DefinitionId = Definition.GetPrimaryAssetId();
    Txt_Name->SetText(Definition.DisplayName.IsEmpty() ? FText::FromName(Definition.GetFName()) : Definition.DisplayName);
    const FAugmentLevelData* Level = Definition.GetLevelData();
    const FText Description = Level ? Level->Description : LOCTEXT("Missing", "No level data. Check this augment definition.");
    Txt_Description->SetText(Description);
    SetToolTipText(FText::Format(LOCTEXT("Tooltip", "{0}\n{1}"), Txt_Name->GetText(), Description));

    FText Tier;
    switch (Definition.Tier)
    {
    case EAugmentTier::Rare: Tier = LOCTEXT("Rare", "RARE"); break;
    case EAugmentTier::Epic: Tier = LOCTEXT("Epic", "EPIC"); break;
    case EAugmentTier::Legendary: Tier = LOCTEXT("Legendary", "LEGENDARY"); break;
    default: Tier = LOCTEXT("Common", "COMMON"); break;
    }
    TierAccent = TrainingAugmentTileStyle::ResolveAccent(Definition.Tier, TierStyles);
    if (const FShooterAugmentTierStyle* Style = TierStyles ? TierStyles->FindStyle(Definition.Tier) : nullptr)
    {
        if (!Style->TierDisplayName.IsEmpty()) { Tier = Style->TierDisplayName; }
    }
    TierAccent.A = 1.0f;
    Txt_Category->SetText(FText::Format(LOCTEXT("Category", "{0}  /  {1}"),
        Definition.AugmentCategory == EAugmentCategory::Active ? LOCTEXT("Active", "ACTIVE") : LOCTEXT("Passive", "PASSIVE"), Tier));
    Txt_Category->SetColorAndOpacity(FSlateColor(TierAccent));
    Txt_Selected->SetColorAndOpacity(FSlateColor(TierAccent));

    if (UTexture2D* Icon = Definition.Icon.LoadSynchronous())
    {
        Img_Icon->SetBrushFromTexture(Icon);
        Img_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else { Img_Icon->SetVisibility(ESlateVisibility::Hidden); }
    SetSelection(false, INDEX_NONE, false);
}

void UTrainingAugmentTileWidget::SetSelection(const bool bSelected, const int32 ActiveIndex, const bool bPending)
{
    if (Btn_Toggle)
    {
        const FButtonStyle Current = Btn_Toggle->GetStyle();
        FButtonStyle Style = TrainingAugmentTileStyle::ApplyTier(bSelected ? SelectedStyle : NormalStyle, TierAccent, bSelected);
        Style.HoveredSlateSound = Current.HoveredSlateSound;
        Style.PressedSlateSound = Current.PressedSlateSound;
        Btn_Toggle->SetStyle(Style);
        Btn_Toggle->SetIsEnabled(!bPending);
    }
    if (Txt_Selected)
    {
        Txt_Selected->SetText(bSelected
            ? (ActiveIndex != INDEX_NONE
                ? FText::Format(LOCTEXT("SelectedSlot", "[X] {0}"), TrainingAugmentRules::GetSlotLabel(ActiveIndex))
                : LOCTEXT("Selected", "[X] SELECTED"))
            : LOCTEXT("Select", "+ SELECT"));
    }
}

void UTrainingAugmentTileWidget::HandleClicked()
{
    if (DefinitionId.IsValid()) { OnClicked.Broadcast(DefinitionId); }
}

#undef LOCTEXT_NAMESPACE
