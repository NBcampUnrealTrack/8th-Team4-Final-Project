#include "Training/UI/TrainingDialogWidget.h"

#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Training/TrainingPlayerComponent.h"

#define LOCTEXT_NAMESPACE "TrainingDialogClasses"

void UTrainingDialogWidget::SetAppliedLoadout(const FTrainingLoadoutSnapshot& Snapshot)
{
    AppliedCombatClass = Snapshot.CombatClass;
    SetAppliedAugments(Snapshot.Augments);
    RefreshClassControls();
}

void UTrainingDialogWidget::BuildClassOptions()
{
    if (!bAutoBindGeneratedUI || !Combo_Class) { return; }
    TGuardValue<bool> UpdateGuard(bUpdatingClassControl, true);
    Combo_Class->ClearOptions();
    ClassOptions.Reset();
    const UDominationClassSettings* Settings = GetDefault<UDominationClassSettings>();
    for (const FDominationCombatClassDefinition& Definition : Settings->CombatClasses)
    {
        if (Definition.CombatClass == EDominationCombatClass::Unselected) { continue; }
        ClassOptions.Add(Definition.CombatClass);
        const FText Name = Definition.DisplayName.IsEmpty()
            ? StaticEnum<EDominationCombatClass>()->GetDisplayNameTextByValue(static_cast<int64>(Definition.CombatClass))
            : Definition.DisplayName;
        Combo_Class->AddOption(Name.ToString());
    }
    RefreshClassControls();
}

void UTrainingDialogWidget::RefreshClassControls()
{
    if (Combo_Class)
    {
        TGuardValue<bool> UpdateGuard(bUpdatingClassControl, true);
        const EDominationCombatClass DisplayClass = PendingCombatClass != EDominationCombatClass::Unselected
            ? PendingCombatClass : AppliedCombatClass;
        const int32 SelectedIndex = ClassOptions.IndexOfByKey(DisplayClass);
        if (SelectedIndex != INDEX_NONE) { Combo_Class->SetSelectedIndex(SelectedIndex); }
        else { Combo_Class->ClearSelection(); }
        Combo_Class->SetIsEnabled(!bRequestPending && !bChoosingAugments && Bridge.IsValid()
            && DialogKind == ETrainingDialogKind::SelectMode && !ClassOptions.IsEmpty());
    }
    if (Txt_ClassHint)
    {
        Txt_ClassHint->SetText(PendingCombatClass != EDominationCombatClass::Unselected
            ? LOCTEXT("Changing", "Applying class and updating augments...")
            : ClassFeedback.IsEmpty()
                ? LOCTEXT("Hint", "Class augments update automatically. Shared augments are kept.") : ClassFeedback);
    }
}

void UTrainingDialogWidget::HandleGeneratedClassSelection(const FString, const ESelectInfo::Type SelectionType)
{
    if (bUpdatingClassControl || SelectionType == ESelectInfo::Direct || !Combo_Class) { return; }
    const int32 Index = Combo_Class->GetSelectedIndex();
    if (bRequestPending || bChoosingAugments || !Bridge.IsValid()
        || DialogKind != ETrainingDialogKind::SelectMode || !ClassOptions.IsValidIndex(Index))
    {
        RefreshClassControls();
        return;
    }
    const EDominationCombatClass RequestedClass = ClassOptions[Index];
    if (RequestedClass == AppliedCombatClass) { return; }
    PendingCombatClass = RequestedClass;
    ClassFeedback = FText::GetEmpty();
    SetRequestPending(true);
    Bridge->SubmitCombatClass(RequestId, RequestedClass);
}

void UTrainingDialogWidget::ReceiveCombatClassResult(const bool bSuccess,
    const FTrainingLoadoutSnapshot& Snapshot, const FText& Failure)
{
    if (PendingCombatClass == EDominationCombatClass::Unselected) { return; }
    const int32 RemovedCount = FMath::Max(0, AppliedAugments.Num() - Snapshot.Augments.Num());
    PendingCombatClass = EDominationCombatClass::Unselected;
    SetAppliedLoadout(Snapshot);
    if (bSuccess)
    {
        ClassFeedback = RemovedCount > 0
            ? FText::Format(LOCTEXT("Removed", "Class updated. {0} incompatible augment(s) removed."), FText::AsNumber(RemovedCount))
            : LOCTEXT("Updated", "Class updated. Available augments now match your class.");
        SetRequestPending(false);
    }
    else
    {
        ClassFeedback = LOCTEXT("Unchanged", "Class change was not applied. Check the message below.");
        ReceiveRequestFailure(Failure);
    }
    if (Combo_Class) { Combo_Class->SetUserFocus(GetOwningPlayer()); }
}

#undef LOCTEXT_NAMESPACE
