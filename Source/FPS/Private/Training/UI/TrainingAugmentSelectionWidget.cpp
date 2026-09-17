#include "Training/UI/TrainingAugmentSelectionWidget.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Weapon/WeaponInventoryComponent.h"
#include "Training/TrainingAugmentRules.h"
#include "Training/TrainingPlayerComponent.h"
#include "Training/UI/TrainingAugmentTileWidget.h"
#include "UI/Common/ShooterAugmentTierStyleSet.h"
#include "TrainingWidgetStyle.h"

#define LOCTEXT_NAMESPACE "TrainingAugmentSelection"

namespace
{
    constexpr EAugmentTier CategoryTiers[] =
    {
        EAugmentTier::Common, EAugmentTier::Rare, EAugmentTier::Epic, EAugmentTier::Legendary
    };

    void StyleCategoryButton(UButton& Button, const FLinearColor& Accent, const bool bSelected)
    {
        FButtonStyle Style = Button.GetStyle();
        const FLinearColor Base(FColor(17, 26, 35));
        const auto MakeBrush = [&Base, &Accent](const float FillAmount, const float EdgeAmount, const float Width)
        {
            FSlateBrush Brush;
            Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
            Brush.TintColor = FSlateColor(FMath::Lerp(Base, Accent, FillAmount));
            Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
            Brush.OutlineSettings.CornerRadii = FVector4(4.0f, 4.0f, 4.0f, 4.0f);
            Brush.OutlineSettings.Color = FSlateColor(FMath::Lerp(Base, Accent, EdgeAmount));
            Brush.OutlineSettings.Width = Width;
            return Brush;
        };
        Style.Normal = MakeBrush(bSelected ? 0.12f : 0.02f, bSelected ? 1.0f : 0.38f, bSelected ? 2.0f : 1.0f);
        Style.Hovered = MakeBrush(0.15f, 1.0f, 2.0f);
        Style.Pressed = MakeBrush(0.20f, 1.0f, 2.0f);
        Style.Disabled = Style.Normal;
        Style.NormalPadding = FMargin(6.0f, 4.0f);
        Style.PressedPadding = Style.NormalPadding;
        Button.SetStyle(Style);
    }
}

void UTrainingAugmentSelectionWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Btn_Apply->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApply);
    Btn_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
    if (Btn_ActiveCommon) { Btn_ActiveCommon->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleActiveCommon); }
    if (Btn_ActiveRare) { Btn_ActiveRare->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleActiveRare); }
    if (Btn_ActiveEpic) { Btn_ActiveEpic->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleActiveEpic); }
    if (Btn_ActiveLegendary) { Btn_ActiveLegendary->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleActiveLegendary); }
    if (Btn_PassiveCommon) { Btn_PassiveCommon->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePassiveCommon); }
    if (Btn_PassiveRare) { Btn_PassiveRare->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePassiveRare); }
    if (Btn_PassiveEpic) { Btn_PassiveEpic->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePassiveEpic); }
    if (Btn_PassiveLegendary) { Btn_PassiveLegendary->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePassiveLegendary); }
}

UAugmentRegistrySubsystem* UTrainingAugmentSelectionWidget::GetRegistry() const
{
    UGameInstance* Instance = GetGameInstance();
    return Instance ? Instance->GetSubsystem<UAugmentRegistrySubsystem>() : nullptr;
}

const UShooterAugmentTierStyleSet* UTrainingAugmentSelectionWidget::GetTierPalette() const
{
    const UTrainingAugmentTileWidget* Defaults = TileWidgetClass ? TileWidgetClass.GetDefaultObject() : nullptr;
    return Defaults ? Defaults->TierStyles.Get() : nullptr;
}

void UTrainingAugmentSelectionWidget::BeginSelection(UTrainingPlayerComponent& InBridge,
    const TArray<FPrimaryAssetId>& AppliedIds)
{
    EndSelection();
    Bridge = &InBridge;
    SelectedIds = AppliedIds;
    ActiveTier = EAugmentTier::Common;
    PassiveTier = EAugmentTier::Common;
    bChooseInitialTiers = true;
    bOpen = true;
    bPending = false;
    bCatalogDirty = false;
    InBridge.ApplyButtonSounds(*this);
    const APlayerController* Controller = GetOwningPlayer();
    const AShooterPlayerState* State = Controller ? Controller->GetPlayerState<AShooterPlayerState>() : nullptr;
    if (State)
    {
        BoundWeapons = State->GetWeaponInventory();
        if (UWeaponInventoryComponent* Weapons = BoundWeapons.Get())
        {
            Weapons->OnWeaponInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleEligibilityChanged);
        }
        BoundAbilitySystem = State->GetAbilitySystemComponent();
        if (UAbilitySystemComponent* ASC = BoundAbilitySystem.Get())
        {
            UnlockChangedHandle = ASC->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::HandleUnlockTagChanged);
        }
    }
    UAugmentRegistrySubsystem* Registry = GetRegistry();
    if (!Registry)
    {
        ClearCatalog();
        ShowFailure(LOCTEXT("NoRegistry", "AugmentRegistrySubsystem is unavailable."));
        RefreshSelection();
        return;
    }
    BoundRegistry = Registry;
    if (Registry->IsReady()) { BuildCatalog(); }
    else
    {
        ClearCatalog();
        ResetCatalogScroll();
        Registry->OnRegistryReady.AddUniqueDynamic(this, &ThisClass::HandleRegistryReady);
        Txt_Status->SetText(LOCTEXT("Loading", "Loading the augment catalog..."));
        RefreshSelection();
    }
}

void UTrainingAugmentSelectionWidget::EndSelection()
{
    if (UAugmentRegistrySubsystem* Registry = BoundRegistry.Get())
    {
        Registry->OnRegistryReady.RemoveDynamic(this, &ThisClass::HandleRegistryReady);
    }
    if (UWeaponInventoryComponent* Weapons = BoundWeapons.Get())
    {
        Weapons->OnWeaponInventoryChanged.RemoveDynamic(this, &ThisClass::HandleEligibilityChanged);
    }
    if (UAbilitySystemComponent* ASC = BoundAbilitySystem.Get())
    {
        ASC->RegisterGenericGameplayTagEvent().Remove(UnlockChangedHandle);
    }
    UnlockChangedHandle.Reset();
    BoundWeapons.Reset();
    BoundAbilitySystem.Reset();
    BoundRegistry.Reset();
    Bridge.Reset();
    bOpen = false;
    bPending = false;
    bCatalogDirty = false;
}

void UTrainingAugmentSelectionWidget::NativeDestruct()
{
    EndSelection();
    Super::NativeDestruct();
}

void UTrainingAugmentSelectionWidget::HandleRegistryReady()
{
    if (bOpen) { BuildCatalog(); }
}

void UTrainingAugmentSelectionWidget::HandleEligibilityChanged()
{
    if (bOpen) { BuildCatalog(); }
}

void UTrainingAugmentSelectionWidget::HandleUnlockTagChanged(const FGameplayTag Tag, const int32)
{
    const FGameplayTag UnlockRoot = FGameplayTag::RequestGameplayTag(TEXT("Unlock"), false);
    if (Tag.MatchesTag(UnlockRoot)) { HandleEligibilityChanged(); }
}

void UTrainingAugmentSelectionWidget::ClearCatalog()
{
    for (UTrainingAugmentTileWidget* Tile : Tiles) { Tile->OnClicked.RemoveAll(this); }
    Tiles.Reset();
    CatalogIds.Reset();
    ActiveTierCounts.Reset();
    PassiveTierCounts.Reset();
    bCatalogBuilt = false;
    List_Active->ClearChildren();
    List_Passive->ClearChildren();
    Txt_ActiveCount->SetText(LOCTEXT("EmptyActive", "ACTIVE / 0"));
    Txt_PassiveCount->SetText(LOCTEXT("EmptyPassive", "PASSIVE / 0"));
    RefreshTierView();
}

void UTrainingAugmentSelectionWidget::ResetCatalogScroll()
{
    for (UScrollBox* Scroll : {Scroll_Active.Get(), Scroll_Passive.Get()})
    {
        if (Scroll)
        {
            Scroll->EndInertialScrolling();
            Scroll->ScrollToStart();
        }
    }
}

void UTrainingAugmentSelectionWidget::BuildCatalog()
{
    UAugmentRegistrySubsystem* Registry = GetRegistry();
    if (!bOpen || !Registry || !Registry->IsReady()) { return; }
    if (bPending) { bCatalogDirty = true; return; }
    bCatalogDirty = false;
    Registry->OnRegistryReady.RemoveDynamic(this, &ThisClass::HandleRegistryReady);
    ClearCatalog();
    FGameplayTagContainer UnlockTags;
    if (!Bridge.IsValid() || !Bridge->GetAugmentAvailabilityTags(UnlockTags))
    {
        ShowFailure(LOCTEXT("UnlockNotReady", "Waiting for your class/weapon unlock state. Reopen the terminal if this persists."));
        RefreshSelection();
        return;
    }
    if (!TileWidgetClass)
    {
        ShowFailure(LOCTEXT("MissingTile", "TileWidgetClass is missing. Assign the augment card WBP in the selection widget class defaults."));
        RefreshSelection();
        return;
    }
    TArray<UAugmentDefinition*> Definitions;
    for (UAugmentDefinition* Definition : Registry->GetAllDefinitions())
    {
        if (Definition && TrainingAugmentRules::IsCatalogEligible(*Definition, UnlockTags))
        {
            Definitions.Add(Definition);
            CatalogIds.Add(Definition->GetPrimaryAssetId());
        }
    }
    const int32 RemovedSelections = SelectedIds.RemoveAll([this](const FPrimaryAssetId& Id)
    {
        return !CatalogIds.Contains(Id);
    });
    Definitions.Sort([](const UAugmentDefinition& A, const UAugmentDefinition& B)
    {
        if (A.Tier != B.Tier) { return A.Tier < B.Tier; }
        const int32 Compare = A.DisplayName.ToString().Compare(B.DisplayName.ToString());
        return Compare != 0 ? Compare < 0 : A.GetName() < B.GetName();
    });
    int32 ActiveCount = 0;
    int32 PassiveCount = 0;
    for (UAugmentDefinition* Definition : Definitions)
    {
        UTrainingAugmentTileWidget* Tile = CreateWidget<UTrainingAugmentTileWidget>(GetOwningPlayer(), TileWidgetClass);
        if (!Tile)
        {
            ShowFailure(LOCTEXT("FailedTile", "Could not create an augment card. Check the generated card Blueprint."));
            RefreshSelection();
            return;
        }
        const bool bActive = Definition->AugmentCategory == EAugmentCategory::Active;
        UVerticalBox* List = bActive ? List_Active.Get() : List_Passive.Get();
        UVerticalBoxSlot* TileSlot = List->AddChildToVerticalBox(Tile);
        TileSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        TileSlot->SetHorizontalAlignment(HAlign_Fill);
        TileSlot->SetVerticalAlignment(VAlign_Top);
        TileSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 12.0f));
        Tile->SetDefinition(*Definition);
        Tile->OnClicked.AddUObject(this, &ThisClass::HandleTileClicked);
        if (Bridge.IsValid()) { Bridge->ApplyButtonSounds(*Tile); }
        Tiles.Add(Tile);
        bActive ? ++ActiveCount : ++PassiveCount;
        TMap<EAugmentTier, int32>& TierCounts = bActive ? ActiveTierCounts : PassiveTierCounts;
        ++TierCounts.FindOrAdd(Definition->Tier);
    }
    Txt_ActiveCount->SetText(FText::Format(LOCTEXT("ActiveCount", "ACTIVE  /  {0}"), FText::AsNumber(ActiveCount)));
    Txt_PassiveCount->SetText(FText::Format(LOCTEXT("PassiveCount", "PASSIVE  /  {0}"), FText::AsNumber(PassiveCount)));
    bCatalogBuilt = true;
    ChooseInitialTiers();
    RefreshTierView();
    Txt_Status->SetText(Definitions.IsEmpty()
        ? LOCTEXT("EmptyCatalog", "No augments are available for your current class/weapon unlocks.")
        : RemovedSelections > 0
            ? LOCTEXT("RemovedLocked", "Unavailable augments were removed from this selection. Apply to update your loadout.")
            : LOCTEXT("Instructions", "Click to select; click again to remove. Changes take effect only after Apply."));
    ResetCatalogScroll();
    RefreshSelection();
}

void UTrainingAugmentSelectionWidget::HandleTileClicked(const FPrimaryAssetId Id)
{
    if (!bOpen || bPending || !bCatalogBuilt || !CatalogIds.Contains(Id)) { return; }
    const TrainingAugmentRules::EToggleResult Result = TrainingAugmentRules::Toggle(SelectedIds, Id);
    Txt_Status->SetText(Result == TrainingAugmentRules::EToggleResult::LimitReached
        ? FText::Format(LOCTEXT("Limit", "Maximum {0} augments. Deselect a card before choosing another one."),
            FText::AsNumber(TrainingAugmentRules::MaximumSelection))
        : LOCTEXT("Instructions", "Click to select; click again to remove. Changes take effect only after Apply."));
    RefreshSelection();
}

void UTrainingAugmentSelectionWidget::RefreshSelection()
{
    const UAugmentRegistrySubsystem* Registry = GetRegistry();
    TMap<FPrimaryAssetId, int32> ActiveIndices;
    TArray<FString> Names;
    int32 ActiveIndex = 0;
    for (const FPrimaryAssetId& Id : SelectedIds)
    {
        const UAugmentDefinition* Definition = Registry ? Registry->FindDefinition(Id) : nullptr;
        FString Name = Definition ? Definition->DisplayName.ToString() : Id.PrimaryAssetName.ToString();
        if (Definition && Definition->AugmentCategory == EAugmentCategory::Active)
        {
            ActiveIndices.Add(Id, ActiveIndex);
            Name += TEXT(" [") + TrainingAugmentRules::GetSlotLabel(ActiveIndex++).ToString() + TEXT("]");
        }
        Names.Add(Name);
    }
    for (UTrainingAugmentTileWidget* Tile : Tiles)
    {
        const FPrimaryAssetId Id = Tile->GetDefinitionId();
        const int32* Index = ActiveIndices.Find(Id);
        Tile->SetSelection(SelectedIds.Contains(Id), Index ? *Index : INDEX_NONE, bPending);
    }
    Txt_Count->SetText(FText::Format(LOCTEXT("Count", "SELECTED  {0} / {1}"),
        FText::AsNumber(SelectedIds.Num()), FText::AsNumber(TrainingAugmentRules::MaximumSelection)));
    Txt_Selection->SetText(Names.IsEmpty() ? LOCTEXT("None", "No augments selected. Apply to clear the current loadout.")
        : FText::FromString(FString::Join(Names, TEXT("   |   "))));
    Btn_Apply->SetIsEnabled(bOpen && bCatalogBuilt && !bPending && SelectedIds.Num() <= TrainingAugmentRules::MaximumSelection);
    Btn_Back->SetIsEnabled(!bPending);
    Txt_Apply->SetText(bPending ? LOCTEXT("Applying", "Applying...") : LOCTEXT("Apply", "Apply augments"));
    RefreshTierButtons();
}

bool UTrainingAugmentSelectionWidget::HasTierCategories() const
{
    for (const EAugmentTier Tier : CategoryTiers)
    {
        if (!GetTierButton(true, Tier) || !GetTierButton(false, Tier)) { return false; }
    }
    return true;
}

UButton* UTrainingAugmentSelectionWidget::GetTierButton(const bool bActive, const EAugmentTier Tier) const
{
    switch (Tier)
    {
    case EAugmentTier::Common: return bActive ? Btn_ActiveCommon.Get() : Btn_PassiveCommon.Get();
    case EAugmentTier::Rare: return bActive ? Btn_ActiveRare.Get() : Btn_PassiveRare.Get();
    case EAugmentTier::Epic: return bActive ? Btn_ActiveEpic.Get() : Btn_PassiveEpic.Get();
    case EAugmentTier::Legendary: return bActive ? Btn_ActiveLegendary.Get() : Btn_PassiveLegendary.Get();
    default: return nullptr;
    }
}

void UTrainingAugmentSelectionWidget::ChooseInitialTiers()
{
    if (!bChooseInitialTiers) { return; }
    for (const bool bActive : {true, false})
    {
        const TMap<EAugmentTier, int32>& Counts = bActive ? ActiveTierCounts : PassiveTierCounts;
        EAugmentTier& VisibleTier = bActive ? ActiveTier : PassiveTier;
        VisibleTier = EAugmentTier::Common;
        for (const EAugmentTier Tier : CategoryTiers)
        {
            if (Counts.FindRef(Tier) > 0) { VisibleTier = Tier; break; }
        }
    }
    bChooseInitialTiers = false;
}

void UTrainingAugmentSelectionWidget::SetVisibleTier(const bool bActive, const EAugmentTier Tier)
{
    if (!bOpen || bPending || !bCatalogBuilt || !HasTierCategories() || !GetTierButton(bActive, Tier)) { return; }
    EAugmentTier& VisibleTier = bActive ? ActiveTier : PassiveTier;
    if (VisibleTier == Tier) { return; }
    VisibleTier = Tier;
    RefreshTierView();
    RefreshTierButtons();
    UScrollBox* Scroll = bActive ? Scroll_Active.Get() : Scroll_Passive.Get();
    if (Scroll)
    {
        Scroll->EndInertialScrolling();
        Scroll->ScrollToStart();
    }
}

void UTrainingAugmentSelectionWidget::RefreshTierView()
{
    const bool bFilterByTier = HasTierCategories();
    const UShooterAugmentTierStyleSet* Palette = GetTierPalette();
    const UAugmentRegistrySubsystem* Registry = GetRegistry();
    for (UTrainingAugmentTileWidget* Tile : Tiles)
    {
        const UAugmentDefinition* Definition = Registry ? Registry->FindDefinition(Tile->GetDefinitionId()) : nullptr;
        const bool bMatches = Definition && Definition->Tier ==
            (Definition->AugmentCategory == EAugmentCategory::Active ? ActiveTier : PassiveTier);
        Tile->SetVisibility(!bFilterByTier || bMatches ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    for (const bool bActive : {true, false})
    {
        const EAugmentTier Tier = bActive ? ActiveTier : PassiveTier;
        const int32 Count = (bActive ? ActiveTierCounts : PassiveTierCounts).FindRef(Tier);
        UTextBlock* Info = bActive ? Txt_ActiveTierInfo.Get() : Txt_PassiveTierInfo.Get();
        UTextBlock* Empty = bActive ? Txt_ActiveTierEmpty.Get() : Txt_PassiveTierEmpty.Get();
        if (Info)
        {
            Info->SetText(bCatalogBuilt
                ? FText::Format(LOCTEXT("TierInfo", "{0}  /  {1} AVAILABLE"), TrainingWidgetStyle::GetAugmentTierName(Tier, Palette), FText::AsNumber(Count))
                : LOCTEXT("TierLoading", "Waiting for the available augment catalog..."));
        }
        if (Empty)
        {
            Empty->SetText(FText::Format(LOCTEXT("TierEmpty", "No {0} augments available.\nChoose another category."), TrainingWidgetStyle::GetAugmentTierName(Tier, Palette)));
            Empty->SetVisibility(bFilterByTier && bCatalogBuilt && Count == 0
                ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
    }
}

void UTrainingAugmentSelectionWidget::RefreshTierButtons()
{
    const UShooterAugmentTierStyleSet* Palette = GetTierPalette();
    for (const bool bActive : {true, false})
    {
        const TMap<EAugmentTier, int32>& Counts = bActive ? ActiveTierCounts : PassiveTierCounts;
        const EAugmentTier VisibleTier = bActive ? ActiveTier : PassiveTier;
        for (const EAugmentTier Tier : CategoryTiers)
        {
            UButton* Button = GetTierButton(bActive, Tier);
            if (!Button) { continue; }
            StyleCategoryButton(*Button, TrainingWidgetStyle::GetAugmentTierColor(Tier, Palette), VisibleTier == Tier);
            Button->SetIsEnabled(bOpen && bCatalogBuilt && !bPending);
            if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
            {
                Label->SetText(FText::Format(LOCTEXT("TierButton", "{0}\n{1} augments"), TrainingWidgetStyle::GetAugmentTierName(Tier, Palette), FText::AsNumber(Counts.FindRef(Tier))));
            }
            Button->SetToolTipText(FText::Format(LOCTEXT("TierTooltip", "Show {0} {1} augments. Your selection in other categories is kept."),
                TrainingWidgetStyle::GetAugmentTierName(Tier, Palette), bActive ? LOCTEXT("ActiveCategory", "active") : LOCTEXT("PassiveCategory", "passive")));
        }
    }
}

void UTrainingAugmentSelectionWidget::HandleActiveCommon() { SetVisibleTier(true, EAugmentTier::Common); }
void UTrainingAugmentSelectionWidget::HandleActiveRare() { SetVisibleTier(true, EAugmentTier::Rare); }
void UTrainingAugmentSelectionWidget::HandleActiveEpic() { SetVisibleTier(true, EAugmentTier::Epic); }
void UTrainingAugmentSelectionWidget::HandleActiveLegendary() { SetVisibleTier(true, EAugmentTier::Legendary); }
void UTrainingAugmentSelectionWidget::HandlePassiveCommon() { SetVisibleTier(false, EAugmentTier::Common); }
void UTrainingAugmentSelectionWidget::HandlePassiveRare() { SetVisibleTier(false, EAugmentTier::Rare); }
void UTrainingAugmentSelectionWidget::HandlePassiveEpic() { SetVisibleTier(false, EAugmentTier::Epic); }
void UTrainingAugmentSelectionWidget::HandlePassiveLegendary() { SetVisibleTier(false, EAugmentTier::Legendary); }

void UTrainingAugmentSelectionWidget::SetPending(const bool bInPending)
{
    if (bPending == bInPending) { return; }
    bPending = bInPending;
    if (bPending) { Txt_Status->SetText(LOCTEXT("Waiting", "Applying on the server...")); }
    if (!bPending && bOpen && bCatalogDirty)
    {
        BuildCatalog();
        return;
    }
    RefreshSelection();
}

void UTrainingAugmentSelectionWidget::ShowFailure(const FText& Failure)
{
    Txt_Status->SetText(Failure);
}

UWidget* UTrainingAugmentSelectionWidget::GetInitialFocusTarget() const { return Btn_Back.Get(); }

void UTrainingAugmentSelectionWidget::HandleApply()
{
    if (bOpen && bCatalogBuilt && !bPending)
    {
        FText Failure;
        if (!TrainingAugmentRules::ValidateIds(SelectedIds, Failure)) { ShowFailure(Failure); return; }
        OnApplyRequested.Broadcast(SelectedIds);
    }
}

void UTrainingAugmentSelectionWidget::HandleBack()
{
    if (bOpen && !bPending) { OnBackRequested.Broadcast(); }
}

#undef LOCTEXT_NAMESPACE
