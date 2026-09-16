#include "Training/UI/TrainingDialogWidget.h"

#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Training/TrainingPlayerComponent.h"
#include "Training/TrainingAugmentRules.h"
#include "Training/UI/TrainingAugmentSelectionWidget.h"

#define LOCTEXT_NAMESPACE "TrainingDialogAugments"

void UTrainingDialogWidget::SetAppliedAugments(const TArray<FAugmentInventoryEntry>& Entries)
{
    AppliedAugments = Entries;
    RefreshAugmentSummary();
}

void UTrainingDialogWidget::RefreshAugmentSummary()
{
    if (!Txt_AugmentSummary) { return; }
    const UGameInstance* Instance = GetGameInstance();
    const UAugmentRegistrySubsystem* Registry = Instance ? Instance->GetSubsystem<UAugmentRegistrySubsystem>() : nullptr;
    TArray<FString> Names;
    for (const FAugmentInventoryEntry& Entry : AppliedAugments)
    {
        const UAugmentDefinition* Definition = Registry ? Registry->FindDefinition(Entry.DefinitionId) : nullptr;
        Names.Add(Definition ? Definition->DisplayName.ToString() : Entry.DefinitionId.PrimaryAssetName.ToString());
    }
    Txt_AugmentSummary->SetText(FText::Format(LOCTEXT("Count", "{0} / {1} applied  |  {2}"),
        FText::AsNumber(AppliedAugments.Num()), FText::AsNumber(TrainingAugmentRules::MaximumSelection), Names.IsEmpty() ? LOCTEXT("None", "No augments")
            : FText::FromString(FString::Join(Names, TEXT(", ")))));
    Txt_AugmentSummary->SetToolTipText(Txt_AugmentSummary->GetText());
}

void UTrainingDialogWidget::OpenAugmentSelection()
{
    if (bRequestPending || bChoosingAugments || !Bridge.IsValid() || DialogKind != ETrainingDialogKind::SelectMode) { return; }
    if (!Switch_TrainingPages || !AugmentSelection)
    {
        ReceiveRequestFailure(LOCTEXT("MissingPage", "The augment selection page is missing. Check Switch_TrainingPages and AugmentSelection in the mode selection WBP."));
        return;
    }
    TArray<FPrimaryAssetId> AppliedIds;
    for (const FAugmentInventoryEntry& Entry : AppliedAugments) { AppliedIds.Add(Entry.DefinitionId); }
    bChoosingAugments = true;
    Switch_TrainingPages->SetActiveWidgetIndex(1);
    AugmentSelection->BeginSelection(*Bridge.Get(), AppliedIds);
    if (UWidget* Focus = AugmentSelection->GetInitialFocusTarget()) { Focus->SetUserFocus(GetOwningPlayer()); }
}

void UTrainingDialogWidget::HandleAugmentApply(const TArray<FPrimaryAssetId>& Selection)
{
    if (bRequestPending || !bChoosingAugments || !Bridge.IsValid()) { return; }
    SetRequestPending(true);
    Bridge->SubmitAugments(RequestId, Selection);
}

void UTrainingDialogWidget::ReceiveAugmentResult(const bool bSuccess,
    const FTrainingLoadoutSnapshot& Snapshot, const FText& Failure)
{
    if (!bChoosingAugments) { return; }
    SetAppliedLoadout(Snapshot);
    SetRequestPending(false);
    if (bSuccess) { HandleAugmentBack(); }
    else if (AugmentSelection) { AugmentSelection->ShowFailure(Failure); }
}

void UTrainingDialogWidget::HandleAugmentBack()
{
    if (bRequestPending || !bChoosingAugments) { return; }
    bChoosingAugments = false;
    if (AugmentSelection) { AugmentSelection->EndSelection(); }
    if (Switch_TrainingPages) { Switch_TrainingPages->SetActiveWidgetIndex(0); }
    RefreshControls();
    if (Btn_Augments) { Btn_Augments->SetUserFocus(GetOwningPlayer()); }
}

#undef LOCTEXT_NAMESPACE
