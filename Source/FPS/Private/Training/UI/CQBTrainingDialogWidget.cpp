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
    if (Btn_Close) Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClose);
    RefreshText();
}

void UCQBTrainingDialogWidget::NativeDestruct()
{
    if (Btn_Primary) Btn_Primary->OnClicked.RemoveDynamic(this, &ThisClass::HandlePrimary);
    if (Btn_Secondary) Btn_Secondary->OnClicked.RemoveDynamic(this, &ThisClass::HandleSecondary);
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
    if (DialogKind == ECQBTrainingDialogKind::Start)
    {
        Bridge->SubmitCQBStart(RequestId);
    }
    else
    {
        Bridge->SubmitCQBEndToStart(RequestId);
    }
}

void UCQBTrainingDialogWidget::HandleSecondary()
{
    if (bRequestPending || !Bridge.IsValid()) return;
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
    const bool bStart = DialogKind == ECQBTrainingDialogKind::Start;
    if (Txt_Title)
    {
        Txt_Title->SetText(bStart
            ? LOCTEXT("StartTitle", "CQB TRAINING / READY")
            : LOCTEXT("EndTitle", "CQB TRAINING / COMPLETE"));
    }
    if (Txt_Prompt)
    {
        Txt_Prompt->SetText(bStart
            ? LOCTEXT("StartPrompt", "Begin the CQB course? Active targets follow the configured CQB unit limit.")
            : LOCTEXT("EndPrompt", "Course clear. Choose where you want to return."));
    }
    if (Txt_Primary)
    {
        Txt_Primary->SetText(bStart
            ? LOCTEXT("StartPrimary", "Start CQB training")
            : LOCTEXT("EndPrimary", "Return to CQB start"));
    }
    if (Txt_Secondary)
    {
        Txt_Secondary->SetText(bStart
            ? LOCTEXT("StartSecondary", "Return to previous position")
            : LOCTEXT("EndSecondary", "Return to original spawn"));
    }
}

void UCQBTrainingDialogWidget::SetPending(const bool bPending)
{
    bRequestPending = bPending;
    if (Btn_Primary) Btn_Primary->SetIsEnabled(!bPending);
    if (Btn_Secondary) Btn_Secondary->SetIsEnabled(!bPending);
    if (Btn_Close) Btn_Close->SetIsEnabled(!bPending);
    if (bPending && Txt_Error)
    {
        Txt_Error->SetVisibility(ESlateVisibility::Collapsed);
    }
}

#undef LOCTEXT_NAMESPACE
