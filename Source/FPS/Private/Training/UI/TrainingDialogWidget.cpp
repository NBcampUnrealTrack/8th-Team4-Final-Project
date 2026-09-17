#include "Training/UI/TrainingDialogWidget.h"

#include "Training/TrainingPlayerComponent.h"
#include "Training/TrainingSettings.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Training/UI/TrainingAugmentSelectionWidget.h"

#define LOCTEXT_NAMESPACE "TrainingDialog"

UTrainingDialogWidget::UTrainingDialogWidget()
{
    InputConfig = EShooterWidgetInputMode::Menu;
    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    SelectedCustomMeters = FMath::Clamp(Settings->DefaultCustomDistanceMeters,
        Settings->GetMinDistanceMeters(), Settings->GetMaxDistanceMeters());
}

void UTrainingDialogWidget::InitializeDialog(UTrainingPlayerComponent& InBridge,
    const ETrainingDialogKind InKind, const int32 InRequestId)
{
    bChoosingAugments = false;
    if (AugmentSelection) { AugmentSelection->EndSelection(); }
    if (Switch_TrainingPages) { Switch_TrainingPages->SetActiveWidgetIndex(0); }
    Bridge = &InBridge;
    DialogKind = InKind;
    RequestId = InRequestId;
    PendingCombatClass = EDominationCombatClass::Unselected;
    ClassFeedback = FText::GetEmpty();
    BuildClassOptions();
    if (bAutoBindGeneratedUI && Txt_Error)
    {
        Txt_Error->SetText(FText::GetEmpty());
        Txt_Error->SetVisibility(ESlateVisibility::Collapsed);
    }
    SetRequestPending(false);
    OnDialogPrepared();
    RefreshControls();
    InBridge.ApplyButtonSounds(*this);
}

void UTrainingDialogWidget::DetachRequest()
{
    if (AugmentSelection) { AugmentSelection->EndSelection(); }
    bChoosingAugments = false;
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
    if (Btn_ReturnStart)
    {
        Btn_ReturnStart->SetIsEnabled(!bPending);
    }
    if (bAutoBindGeneratedUI && bPending && Txt_Error)
    {
        Txt_Error->SetText(FText::GetEmpty());
        Txt_Error->SetVisibility(ESlateVisibility::Collapsed);
    }
    RefreshControls();
    if (AugmentSelection && bChoosingAugments) { AugmentSelection->SetPending(bPending); }
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
    if (!bRequestPending && !bChoosingAugments && Bridge.IsValid() && DialogKind == ETrainingDialogKind::SelectMode)
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
    if (bRequestPending) { return; }
    if (bChoosingAugments) { HandleAugmentBack(); return; }
    if (Bridge.IsValid())
    {
        Bridge->DismissDialog(RequestId);
    }
    else
    {
        DeactivateWidget();
    }
}

void UTrainingDialogWidget::ReturnToStartPosition()
{
    if (bRequestPending || bChoosingAugments || !Bridge.IsValid()
        || DialogKind != ETrainingDialogKind::SelectMode)
    {
        return;
    }

    SetRequestPending(true);
    Bridge->ReturnToPreTrainingPosition(RequestId);
}

void UTrainingDialogWidget::NativeDestruct()
{
    if (AugmentSelection) { AugmentSelection->EndSelection(); }
    UnbindControls();
    if (Bridge.IsValid())
    {
        Bridge->NotifyDialogRemoved(*this);
    }
    Super::NativeDestruct();
}

bool UTrainingDialogWidget::NativeOnHandleBackAction()
{
    CancelDialog();
    return true;
}

void UTrainingDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (Bridge.IsValid()) { Bridge->ApplyButtonSounds(*this); }

    if (!Btn_ReturnStart)
    {
        Btn_ReturnStart = Cast<UButton>(GetWidgetFromName(TEXT("Btn_ReturnStart")));
    }
    if (Btn_ReturnStart)
    {
        Btn_ReturnStart->OnClicked.AddUniqueDynamic(this, &ThisClass::ReturnToStartPosition);
    }

    if (!bAutoBindGeneratedUI)
    {
        return;
    }
    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    if (RequestId == 0)
    {
        DialogKind = GeneratedDialogKind;
    }
    BindControls();
    BuildClassOptions();
    if (Switch_TrainingPages) { Switch_TrainingPages->SetActiveWidgetIndex(bChoosingAugments ? 1 : 0); }
    if (UTextBlock* TargetDescription = Cast<UTextBlock>(GetWidgetFromName(TEXT("Btn_ModeTarget_Description"))))
    {
        TargetDescription->SetText(FText::Format(LOCTEXT("LinearTargetDescription",
            "Stationary targets. Spawn interval: {0}s to {1}s.\nOne hit removes a target. {2}-second session."),
            FText::AsNumber(Settings->TargetSpawnIntervalSeconds),
            FText::AsNumber(Settings->TargetMinSpawnIntervalSeconds),
            FText::AsNumber(Settings->TargetDurationSeconds)));
    }
    if (UTextBlock* TargetFooter = Cast<UTextBlock>(GetWidgetFromName(TEXT("Btn_ModeTarget_Foot"))))
    {
        TargetFooter->SetText(LOCTEXT("StationaryTargetFooter", "ONE HIT   /   STATIONARY   /   AUTO END"));
    }
    if (Spin_CustomMeters)
    {
        SelectedCustomMeters = FMath::Clamp(Spin_CustomMeters->GetValue(), Settings->GetMinDistanceMeters(), Settings->GetMaxDistanceMeters());
    }
    RefreshControls();
}

UWidget* UTrainingDialogWidget::NativeGetDesiredFocusTarget() const
{
    if (bChoosingAugments && AugmentSelection) { return AugmentSelection->GetInitialFocusTarget(); }
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

void UTrainingDialogWidget::BindControls()
{
    if (!bAutoBindGeneratedUI)
    {
        return;
    }
    if (Btn_Augments) { Btn_Augments->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenAugmentSelection); }
    if (Combo_Class) { Combo_Class->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleGeneratedClassSelection); }
    if (AugmentSelection)
    {
        AugmentSelection->OnApplyRequested.RemoveAll(this);
        AugmentSelection->OnBackRequested.RemoveAll(this);
        AugmentSelection->OnApplyRequested.AddUObject(this, &ThisClass::HandleAugmentApply);
        AugmentSelection->OnBackRequested.AddUObject(this, &ThisClass::HandleAugmentBack);
    }
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

void UTrainingDialogWidget::UnbindControls()
{
    if (Btn_ReturnStart)
    {
        Btn_ReturnStart->OnClicked.RemoveDynamic(this, &ThisClass::ReturnToStartPosition);
    }

    if (!bAutoBindGeneratedUI)
    {
        return;
    }
    if (Btn_Augments) { Btn_Augments->OnClicked.RemoveDynamic(this, &ThisClass::OpenAugmentSelection); }
    if (Combo_Class) { Combo_Class->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleGeneratedClassSelection); }
    if (AugmentSelection)
    {
        AugmentSelection->OnApplyRequested.RemoveAll(this);
        AugmentSelection->OnBackRequested.RemoveAll(this);
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

void UTrainingDialogWidget::RefreshControls()
{
    if (!bAutoBindGeneratedUI)
    {
        return;
    }
    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    RefreshAugmentSummary();
    RefreshClassControls();
    if (Txt_Prompt) Txt_Prompt->SetText(GetPromptText());
    if (Txt_Primary)
    {
        Txt_Primary->SetText(bRequestPending ? LOCTEXT("Requesting", "Requesting...") : PrimaryActionText);
    }
    UButton* Buttons[] = { Btn_Primary.Get(), Btn_ReturnStart.Get(), Btn_Cancel.Get(), Btn_Close.Get(),
        Btn_ModeTarget.Get(), Btn_ModeCombat.Get(), Btn_Distance10.Get(),
        Btn_Distance20.Get(), Btn_Distance30.Get(), Btn_DistanceCustom.Get(), Btn_Augments.Get() };
    for (UButton* ChoiceButton : Buttons)
    {
        if (ChoiceButton) ChoiceButton->SetIsEnabled(!bRequestPending);
    }
    const auto ApplyChoice = [this](UButton* ChoiceButton, const bool bSelected)
    {
        if (!ChoiceButton) { return; }
        const FButtonStyle& Current = ChoiceButton->GetStyle();
        FButtonStyle Style = bSelected ? ChoiceSelectedStyle : ChoiceNormalStyle;
        Style.HoveredSlateSound = Current.HoveredSlateSound;
        Style.PressedSlateSound = Current.PressedSlateSound;
        ChoiceButton->SetStyle(Style);
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
                FText::AsNumber(ResolvedBand.MinCentimeters / TrainingRules::CentimetersPerMeter),
                FText::AsNumber(ResolvedBand.MaxCentimeters / TrainingRules::CentimetersPerMeter)));
        }
    }
    if (Txt_SessionHint)
    {
        Txt_SessionHint->SetText(SelectedMode == ETrainingMode::TargetAcquisition
            ? FText::Format(LOCTEXT("TargetHintLinearSpawn",
                "{0}s countdown  /  {1}s session  /  Spawns: {2}s to {3}s"),
                FText::AsNumber(Settings->GetCountdownSeconds()),
                FText::AsNumber(Settings->TargetDurationSeconds),
                FText::AsNumber(Settings->TargetSpawnIntervalSeconds),
                FText::AsNumber(Settings->TargetMinSpawnIntervalSeconds))
            : FText::Format(LOCTEXT("CombatHint", "{0}s countdown  /  One AI every {1}s  /  Max {2} alive"),
                FText::AsNumber(Settings->GetCountdownSeconds()),
                FText::AsNumber(Settings->CombatSpawnIntervalSeconds),
                FText::AsNumber(Settings->MaxLivingCombatAI)));
    }
}

void UTrainingDialogWidget::HandleGeneratedPrimary()
{
    if (DialogKind == ETrainingDialogKind::SelectMode)
    {
        const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
        const float CustomValue = Spin_CustomMeters ? Spin_CustomMeters->GetValue() : SelectedCustomMeters;
        StartSelectedMode(SelectedMode, SelectedPreset, FMath::Clamp(CustomValue, Settings->GetMinDistanceMeters(), Settings->GetMaxDistanceMeters()));
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
    RefreshControls();
}

void UTrainingDialogWidget::HandleGeneratedCombat()
{
    if (bRequestPending) return;
    SelectedMode = ETrainingMode::CombatResponse;
    RefreshControls();
}

void UTrainingDialogWidget::SelectDistance(const ETrainingDistancePreset NewPreset)
{
    if (bRequestPending) return;
    SelectedPreset = NewPreset;
    RefreshControls();
}

void UTrainingDialogWidget::HandleGenerated10() { SelectDistance(ETrainingDistancePreset::TenMeters); }
void UTrainingDialogWidget::HandleGenerated20() { SelectDistance(ETrainingDistancePreset::TwentyMeters); }
void UTrainingDialogWidget::HandleGenerated30() { SelectDistance(ETrainingDistancePreset::ThirtyMeters); }
void UTrainingDialogWidget::HandleGeneratedCustom() { SelectDistance(ETrainingDistancePreset::Custom); }

void UTrainingDialogWidget::HandleGeneratedCustomValue(const float NewValue)
{
    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    if (bRequestPending || !FMath::IsFinite(NewValue)) return;
    SelectedCustomMeters = FMath::Clamp(NewValue, Settings->GetMinDistanceMeters(), Settings->GetMaxDistanceMeters());
    RefreshControls();
}

#undef LOCTEXT_NAMESPACE
