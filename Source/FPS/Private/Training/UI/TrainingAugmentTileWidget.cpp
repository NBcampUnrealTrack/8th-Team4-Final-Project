#include "Training/UI/TrainingAugmentTileWidget.h"

#include "Augment/AugmentDefinition.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Training/TrainingAugmentRules.h"
#include "TrainingWidgetStyle.h"

#define LOCTEXT_NAMESPACE "TrainingAugmentTile"

namespace TrainingAugmentTileStyle
{
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
    Btn_Toggle->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UTrainingAugmentTileWidget::SetDefinition(UAugmentDefinition& Definition)
{
    DefinitionId = Definition.GetPrimaryAssetId();
    Txt_Name->SetText(Definition.DisplayName.IsEmpty() ? FText::FromName(Definition.GetFName()) : Definition.DisplayName);
    const FAugmentLevelData* Level = Definition.GetLevelData();
    const FText Description = Level ? Level->Description : LOCTEXT("Missing", "No level data. Check this augment definition.");
    Txt_Description->SetText(Description);
    SetToolTipText(FText::Format(LOCTEXT("Tooltip", "{0}\n{1}"), Txt_Name->GetText(), Description));

    const FText Tier = TrainingWidgetStyle::GetAugmentTierName(Definition.Tier, TierStyles);
    TierAccent = TrainingWidgetStyle::GetAugmentTierColor(Definition.Tier, TierStyles);
    Txt_Category->SetText(FText::Format(LOCTEXT("Category", "{0}  /  {1}"),
        Definition.AugmentCategory == EAugmentCategory::Active ? LOCTEXT("Active", "ACTIVE") : LOCTEXT("Passive", "PASSIVE"), Tier));
    Txt_Category->SetColorAndOpacity(FSlateColor(TierAccent));
    Txt_Selected->SetColorAndOpacity(FSlateColor(TierAccent));

    Img_Icon->SetBrushFromSoftTexture(Definition.Icon, false);
    Img_Icon->SetVisibility(Definition.Icon.IsNull() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
    bSelectionInitialized = false;
    SetSelection(false, INDEX_NONE, false);
}

void UTrainingAugmentTileWidget::SetSelection(const bool bSelected, const int32 ActiveIndex, const bool bPending)
{
    Btn_Toggle->SetIsEnabled(!bPending);
    if (bSelectionInitialized && bLastSelected == bSelected && LastActiveIndex == ActiveIndex) { return; }
    bSelectionInitialized = true;
    bLastSelected = bSelected;
    LastActiveIndex = ActiveIndex;
    const FButtonStyle& Current = Btn_Toggle->GetStyle();
    FButtonStyle Style = TrainingAugmentTileStyle::ApplyTier(bSelected ? SelectedStyle : NormalStyle, TierAccent, bSelected);
    Style.HoveredSlateSound = Current.HoveredSlateSound;
    Style.PressedSlateSound = Current.PressedSlateSound;
    Btn_Toggle->SetStyle(Style);
    Txt_Selected->SetText(bSelected
        ? (ActiveIndex != INDEX_NONE
            ? FText::Format(LOCTEXT("SelectedSlot", "[X] {0}"), TrainingAugmentRules::GetSlotLabel(ActiveIndex))
            : LOCTEXT("Selected", "[X] SELECTED"))
        : LOCTEXT("Select", "+ SELECT"));
}

void UTrainingAugmentTileWidget::HandleClicked()
{
    if (DefinitionId.IsValid()) { OnClicked.Broadcast(DefinitionId); }
}

#undef LOCTEXT_NAMESPACE
