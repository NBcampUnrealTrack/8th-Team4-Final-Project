#include "Training/UI/CQBTrainingDialogWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Training/TrainingPlayerComponent.h"

#define LOCTEXT_NAMESPACE "CQBTrainingDialog"

UCQBTrainingDialogWidget::UCQBTrainingDialogWidget()
{
    InputConfig = EShooterWidgetInputMode::Menu;
}

void UCQBTrainingDialogWidget::InitializeDialog(
    UTrainingPlayerComponent& InBridge, const ECQBTrainingDialogKind InKind, const int32 InRequestId)
{
    Bridge = &InBridge;
    DialogKind = InKind;
    RequestId = InRequestId;

    SetPending(false);
    if (Txt_Error)
    {
        Txt_Error->SetText(FText::GetEmpty());
        Txt_Error->SetVisibility(ESlateVisibility::Collapsed);
    }

    RefreshText();
    InBridge.ApplyButtonSounds(*this);
}

void UCQBTrainingDialogWidget::DetachRequest()
{
    Bridge.Reset();
    RequestId = 0;
}

void UCQBTrainingDialogWidget::ReceiveRequestFailure(const FText& Failure)
{
    SetPending(false);
    if (Txt_Error)
    {
        Txt_Error->SetText(Failure);
        Txt_Error->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UCQBTrainingDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Primary) Btn_Primary->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePrimary);
    if (Btn_Secondary) Btn_Secondary->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSecondary);
    if (Btn_Class) Btn_Class->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClassSelection);
    if (Btn_Close) Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClose);

    RefreshText();
}

void UCQBTrainingDialogWidget::NativeDestruct()
{
    if (Btn_Primary) Btn_Primary->OnClicked.RemoveDynamic(this, &ThisClass::HandlePrimary);
    if (Btn_Secondary) Btn_Secondary->OnClicked.RemoveDynamic(this, &ThisClass::HandleSecondary);
    if (Btn_Class) Btn_Class->OnClicked.RemoveDynamic(this, &ThisClass::HandleClassSelection);
    if (Btn_Close) Btn_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleClose);

    if (Bridge.IsValid())
    {
        Bridge->NotifyCQBDialogRemoved(*this);
    }

    Super::NativeDestruct();
}

bool UCQBTrainingDialogWidget::NativeOnHandleBackAction()
{
    HandleClose();
    return true;
}

UWidget* UCQBTrainingDialogWidget::NativeGetDesiredFocusTarget() const
{
    return Btn_Primary ? Btn_Primary.Get() : Super::NativeGetDesiredFocusTarget();
}

void UCQBTrainingDialogWidget::HandlePrimary()
{
    if (bRequestPending || !Bridge.IsValid()) return;
    SetPending(true);

    switch (DialogKind)
    {
    case ECQBTrainingDialogKind::Start:
        Bridge->SubmitCQBStart(RequestId);
        break;
    case ECQBTrainingDialogKind::Cancel:
        Bridge->SubmitCQBCancel(RequestId);
        break;
    case ECQBTrainingDialogKind::End:
        Bridge->SubmitCQBEndToStart(RequestId);
        break;
    }
}

void UCQBTrainingDialogWidget::HandleSecondary()
{
    if (bRequestPending || !Bridge.IsValid()) return;

    if (DialogKind == ECQBTrainingDialogKind::Cancel)
    {
        Bridge->DismissCQBDialog(RequestId);
        return;
    }

    SetPending(true);
    if (DialogKind == ECQBTrainingDialogKind::Start)
    {
        Bridge->SubmitCQBReturnToOrigin(RequestId);
    }
    else
    {
        Bridge->SubmitCQBEndToOrigin(RequestId);
    }
}

void UCQBTrainingDialogWidget::HandleClassSelection()
{
    if (bRequestPending || DialogKind != ECQBTrainingDialogKind::Start || !Bridge.IsValid()) return;
    SetPending(true);
    Bridge->SubmitCQBClassSelection(RequestId);
}

void UCQBTrainingDialogWidget::HandleClose()
{
    if (bRequestPending) return;

    if (Bridge.IsValid())
    {
        Bridge->DismissCQBDialog(RequestId);
    }
    else
    {
        DeactivateWidget();
    }
}

void UCQBTrainingDialogWidget::RefreshText()
{
    FText Title;
    FText Prompt;
    FText Primary;
    FText Secondary;

    switch (DialogKind)
    {
    case ECQBTrainingDialogKind::Start:
        Title = LOCTEXT("StartTitle", "CQB TRAINING / READY");
        Prompt = LOCTEXT("StartPrompt",
            "Begin the CQB course? Active targets follow the configured CQB unit limit.");
        Primary = LOCTEXT("StartPrimary", "Start CQB training");
        Secondary = LOCTEXT("StartSecondary", "Return to previous position");
        break;
    case ECQBTrainingDialogKind::Cancel:
        Title = LOCTEXT("CancelTitle", "CQB TRAINING / CANCEL");
        Prompt = LOCTEXT("CancelPrompt",
            "Cancel the current CQB course? Active targets and current progress will be cleared.");
        Primary = LOCTEXT("CancelPrimary", "Cancel CQB training");
        Secondary = LOCTEXT("CancelSecondary", "Continue training");
        break;
    case ECQBTrainingDialogKind::End:
        Title = LOCTEXT("EndTitle", "CQB TRAINING / COMPLETE");
        Prompt = LOCTEXT("EndPrompt", "Course clear. Choose where you want to return.");
        Primary = LOCTEXT("EndPrimary", "Return to CQB start");
        Secondary = LOCTEXT("EndSecondary", "Return to original spawn");
        break;
    }

    if (Txt_Title) Txt_Title->SetText(Title);
    if (Txt_Prompt) Txt_Prompt->SetText(Prompt);
    if (Txt_Primary) Txt_Primary->SetText(Primary);
    if (Txt_Secondary) Txt_Secondary->SetText(Secondary);
    if (Txt_Class) Txt_Class->SetText(LOCTEXT("ClassSelection", "SELECT CLASS"));
    if (Btn_Class)
    {
        Btn_Class->SetVisibility(DialogKind == ECQBTrainingDialogKind::Start
            ? ESlateVisibility::Visible
            : ESlateVisibility::Collapsed);
    }
}

void UCQBTrainingDialogWidget::SetPending(const bool bPending)
{
    bRequestPending = bPending;
    if (Btn_Primary) Btn_Primary->SetIsEnabled(!bPending);
    if (Btn_Secondary) Btn_Secondary->SetIsEnabled(!bPending);
    if (Btn_Class) Btn_Class->SetIsEnabled(!bPending);
    if (Btn_Close) Btn_Close->SetIsEnabled(!bPending);

    if (bPending && Txt_Error)
    {
        Txt_Error->SetVisibility(ESlateVisibility::Collapsed);
    }
}

#undef LOCTEXT_NAMESPACE
