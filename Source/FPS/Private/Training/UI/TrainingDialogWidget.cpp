#include "Training/UI/TrainingDialogWidget.h"

#include "Training/TrainingPlayerComponent.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "TrainingDialog"

UTrainingDialogWidget::UTrainingDialogWidget()
{
    InputConfig = EShooterWidgetInputMode::Menu;
}

void UTrainingDialogWidget::InitializeDialog(UTrainingPlayerComponent& InBridge,
    const ETrainingDialogKind InKind, const int32 InRequestId)
{
    Bridge = &InBridge;
    DialogKind = InKind;
    RequestId = InRequestId;
    if (bAutoBindGeneratedUI && Txt_Error)
    {
        Txt_Error->SetText(FText::GetEmpty());
        Txt_Error->SetVisibility(ESlateVisibility::Collapsed);
    }
    SetRequestPending(false);
    OnDialogPrepared();
    RefreshGeneratedControls();
}

void UTrainingDialogWidget::DetachRequest()
{
    Bridge.Reset();
    RequestId = 0;
}

FText UTrainingDialogWidget::GetPromptText() const
{
    switch (DialogKind)
    {
    case ETrainingDialogKind::EnterArena:
        return LOCTEXT("Enter", "Do you want to enter the Training Arena?");
    case ETrainingDialogKind::EndSession:
        return LOCTEXT("End", "Do you want to end the current training session?");
    default:
        return LOCTEXT("Select", "Choose a training mode and distance.");
    }
}

void UTrainingDialogWidget::SetRequestPending(const bool bPending)
{
    bRequestPending = bPending;
    if (bAutoBindGeneratedUI && bPending && Txt_Error)
    {
        Txt_Error->SetText(FText::GetEmpty());
        Txt_Error->SetVisibility(ESlateVisibility::Collapsed);
    }
    RefreshGeneratedControls();
    OnRequestPendingChanged(bPending);
}

void UTrainingDialogWidget::ConfirmDialog()
{
    if (!bRequestPending && Bridge.IsValid() && DialogKind != ETrainingDialogKind::SelectMode)
    {
        SetRequestPending(true);
        Bridge->SubmitConfirmation(RequestId);
    }
}

void UTrainingDialogWidget::StartSelectedMode(const ETrainingMode Mode,
    const ETrainingDistancePreset Preset, const float CustomMeters)
{
    if (!bRequestPending && Bridge.IsValid() && DialogKind == ETrainingDialogKind::SelectMode)
    {
        SetRequestPending(true);
        Bridge->SubmitMode(RequestId, Mode, Preset, CustomMeters);
    }
}

void UTrainingDialogWidget::ReceiveRequestFailure(const FText& Failure)
{
    SetRequestPending(false);
    if (bAutoBindGeneratedUI && Txt_Error)
    {
        Txt_Error->SetText(Failure);
        Txt_Error->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    OnRequestFailed(Failure);
}

void UTrainingDialogWidget::CancelDialog()
{
    if (Bridge.IsValid())
    {
        Bridge->DismissDialog(RequestId);
    }
    else
    {
        DeactivateWidget();
    }
}

void UTrainingDialogWidget::NativeDestruct()
{
    UnbindGeneratedControls();
    if (Bridge.IsValid())
    {
        Bridge->NotifyDialogRemoved(*this);
    }
    Super::NativeDestruct();
}

bool UTrainingDialogWidget::NativeOnHandleBackAction()
{
    // Do not dismiss a confirmation after its authoritative request was sent.
    if (bAutoBindGeneratedUI && bRequestPending)
    {
        return true;
    }
    CancelDialog();
    return true;
}


void UTrainingDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!bAutoBindGeneratedUI)
    {
        return;
    }
    if (RequestId == 0)
    {
        DialogKind = GeneratedDialogKind;
    }
    BindGeneratedControls();
    if (Spin_CustomMeters)
    {
        SelectedCustomMeters = FMath::Clamp(Spin_CustomMeters->GetValue(), 5.0f, 50.0f);
    }
    RefreshGeneratedControls();
}

UWidget* UTrainingDialogWidget::NativeGetDesiredFocusTarget() const
{
    // Ending training is destructive; put initial keyboard/gamepad focus on Back.
    if (bAutoBindGeneratedUI)
    {
        if ((RequestId > 0 ? DialogKind : GeneratedDialogKind) == ETrainingDialogKind::EndSession && Btn_Cancel)
        {
            return Btn_Cancel;
        }
        if (Btn_Primary)
        {
            return Btn_Primary;
        }
    }
    return Super::NativeGetDesiredFocusTarget();
}

void UTrainingDialogWidget::BindGeneratedControls()
{
    if (!bAutoBindGeneratedUI)
    {
        return;
    }
    // AddUnique permits a CommonUI widget to be constructed again without duplicate clicks.
    if (Btn_Primary) Btn_Primary->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGeneratedPrimary);
    if (Btn_Cancel) Btn_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::CancelDialog);
    if (Btn_Close) Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::CancelDialog);
    if (Btn_ModeTarget) Btn_ModeTarget->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGeneratedTarget);
    if (Btn_ModeCombat) Btn_ModeCombat->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGeneratedCombat);
    if (Btn_Distance10) Btn_Distance10->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGenerated10);
    if (Btn_Distance20) Btn_Distance20->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGenerated20);
    if (Btn_Distance30) Btn_Distance30->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGenerated30);
    if (Btn_DistanceCustom) Btn_DistanceCustom->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGeneratedCustom);
    if (Spin_CustomMeters) Spin_CustomMeters->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleGeneratedCustomValue);
}

void UTrainingDialogWidget::UnbindGeneratedControls()
{
    if (!bAutoBindGeneratedUI)
    {
        return;
    }
    if (Btn_Primary) Btn_Primary->OnClicked.RemoveDynamic(this, &ThisClass::HandleGeneratedPrimary);
    if (Btn_Cancel) Btn_Cancel->OnClicked.RemoveDynamic(this, &ThisClass::CancelDialog);
    if (Btn_Close) Btn_Close->OnClicked.RemoveDynamic(this, &ThisClass::CancelDialog);
    if (Btn_ModeTarget) Btn_ModeTarget->OnClicked.RemoveDynamic(this, &ThisClass::HandleGeneratedTarget);
    if (Btn_ModeCombat) Btn_ModeCombat->OnClicked.RemoveDynamic(this, &ThisClass::HandleGeneratedCombat);
    if (Btn_Distance10) Btn_Distance10->OnClicked.RemoveDynamic(this, &ThisClass::HandleGenerated10);
    if (Btn_Distance20) Btn_Distance20->OnClicked.RemoveDynamic(this, &ThisClass::HandleGenerated20);
    if (Btn_Distance30) Btn_Distance30->OnClicked.RemoveDynamic(this, &ThisClass::HandleGenerated30);
    if (Btn_DistanceCustom) Btn_DistanceCustom->OnClicked.RemoveDynamic(this, &ThisClass::HandleGeneratedCustom);
    if (Spin_CustomMeters) Spin_CustomMeters->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleGeneratedCustomValue);
}

void UTrainingDialogWidget::RefreshGeneratedControls()
{
    if (!bAutoBindGeneratedUI)
    {
        return;
    }
    if (Txt_Prompt) Txt_Prompt->SetText(GetPromptText());
    if (Txt_Primary)
    {
        Txt_Primary->SetText(bRequestPending ? LOCTEXT("Requesting", "Requesting...") : PrimaryActionText);
    }
    UButton* Buttons[] = { Btn_Primary.Get(), Btn_Cancel.Get(), Btn_Close.Get(),
        Btn_ModeTarget.Get(), Btn_ModeCombat.Get(), Btn_Distance10.Get(),
        Btn_Distance20.Get(), Btn_Distance30.Get(), Btn_DistanceCustom.Get() };
    for (UButton* ChoiceButton : Buttons)
    {
        if (ChoiceButton) ChoiceButton->SetIsEnabled(!bRequestPending);
    }
    const auto ApplyChoice = [this](UButton* ChoiceButton, const bool bSelected)
    {
        if (ChoiceButton) ChoiceButton->SetStyle(bSelected ? ChoiceSelectedStyle : ChoiceNormalStyle);
    };
    ApplyChoice(Btn_ModeTarget.Get(), SelectedMode == ETrainingMode::TargetAcquisition);
    ApplyChoice(Btn_ModeCombat.Get(), SelectedMode == ETrainingMode::CombatResponse);
    ApplyChoice(Btn_Distance10.Get(), SelectedPreset == ETrainingDistancePreset::TenMeters);
    ApplyChoice(Btn_Distance20.Get(), SelectedPreset == ETrainingDistancePreset::TwentyMeters);
    ApplyChoice(Btn_Distance30.Get(), SelectedPreset == ETrainingDistancePreset::ThirtyMeters);
    ApplyChoice(Btn_DistanceCustom.Get(), SelectedPreset == ETrainingDistancePreset::Custom);
    if (Img_TargetSelected)
    {
        Img_TargetSelected->SetVisibility(SelectedMode == ETrainingMode::TargetAcquisition
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (Img_CombatSelected)
    {
        Img_CombatSelected->SetVisibility(SelectedMode == ETrainingMode::CombatResponse
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (Spin_CustomMeters)
    {
        Spin_CustomMeters->SetIsEnabled(!bRequestPending && SelectedPreset == ETrainingDistancePreset::Custom);
    }
    if (Txt_Range)
    {
        FTrainingDistanceBand ResolvedBand;
        if (TrainingRules::ResolveDistanceBand(SelectedPreset, SelectedCustomMeters, ResolvedBand))
        {
            Txt_Range->SetText(FText::Format(LOCTEXT("Range", "Spawn range: {0}-{1} m  |  Fixed arena center"),
                FText::AsNumber(ResolvedBand.MinCentimeters / 100.0f),
                FText::AsNumber(ResolvedBand.MaxCentimeters / 100.0f)));
        }
    }
    if (Txt_SessionHint)
    {
        Txt_SessionHint->SetText(SelectedMode == ETrainingMode::TargetAcquisition
            ? LOCTEXT("TargetHint", "60-second session  /  One target every 2 seconds")
            : LOCTEXT("CombatHint", "Manual end  /  One AI every 5 seconds  /  Max 10 alive"));
    }
}

void UTrainingDialogWidget::HandleGeneratedPrimary()
{
    if (DialogKind == ETrainingDialogKind::SelectMode)
    {
        // Read the final committed value rather than relying on the last slider event.
        const float CustomValue = Spin_CustomMeters ? Spin_CustomMeters->GetValue() : SelectedCustomMeters;
        StartSelectedMode(SelectedMode, SelectedPreset, FMath::Clamp(CustomValue, 5.0f, 50.0f));
    }
    else
    {
        ConfirmDialog();
    }
}

void UTrainingDialogWidget::HandleGeneratedTarget()
{
    if (bRequestPending) return;
    SelectedMode = ETrainingMode::TargetAcquisition;
    RefreshGeneratedControls();
}

void UTrainingDialogWidget::HandleGeneratedCombat()
{
    if (bRequestPending) return;
    SelectedMode = ETrainingMode::CombatResponse;
    RefreshGeneratedControls();
}

void UTrainingDialogWidget::SelectGeneratedDistance(const ETrainingDistancePreset NewPreset)
{
    if (bRequestPending) return;
    SelectedPreset = NewPreset;
    RefreshGeneratedControls();
}

void UTrainingDialogWidget::HandleGenerated10() { SelectGeneratedDistance(ETrainingDistancePreset::TenMeters); }
void UTrainingDialogWidget::HandleGenerated20() { SelectGeneratedDistance(ETrainingDistancePreset::TwentyMeters); }
void UTrainingDialogWidget::HandleGenerated30() { SelectGeneratedDistance(ETrainingDistancePreset::ThirtyMeters); }
void UTrainingDialogWidget::HandleGeneratedCustom() { SelectGeneratedDistance(ETrainingDistancePreset::Custom); }

void UTrainingDialogWidget::HandleGeneratedCustomValue(const float NewValue)
{
    if (bRequestPending || !FMath::IsFinite(NewValue)) return;
    SelectedCustomMeters = FMath::Clamp(NewValue, 5.0f, 50.0f);
    RefreshGeneratedControls();
}

#undef LOCTEXT_NAMESPACE
