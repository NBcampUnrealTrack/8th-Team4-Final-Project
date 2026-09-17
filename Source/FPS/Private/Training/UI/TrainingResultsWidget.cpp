#include "Training/UI/TrainingResultsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Training/TrainingPlayerComponent.h"
#include "TrainingWidgetLayout.h"

UTrainingResultsWidget::UTrainingResultsWidget()
{
    InputConfig = EShooterWidgetInputMode::Menu;
}

TSharedRef<SWidget> UTrainingResultsWidget::RebuildWidget()
{
    if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient); }
    if (!WidgetTree->RootWidget) { TrainingWidgetLayout::BuildResults(*WidgetTree); }
    TrainingWidgetLayout::RepairTextFonts(*WidgetTree);
    Btn_Confirm = Cast<UButton>(WidgetTree->FindWidget(TEXT("Btn_Confirm")));
    Txt_ResultTitle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_ResultTitle")));
    Txt_ResultMode = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_ResultMode")));
    Txt_ResultDuration = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_ResultDuration")));
    Txt_ResultKills = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_ResultKills")));
    Txt_ResultAccuracy = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_ResultAccuracy")));
    Txt_ResultShots = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_ResultShots")));
    return Super::RebuildWidget();
}

void UTrainingResultsWidget::InitializeResults(UTrainingPlayerComponent& InBridge,
    const FTrainingSessionResult& InResult)
{
    Bridge = &InBridge;
    Result = InResult;
    RefreshResult();
    InBridge.ApplyButtonSounds(*this);
}

void UTrainingResultsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (WidgetTree) { TrainingWidgetLayout::RepairTextFonts(*WidgetTree); }
    if (Btn_Confirm) { Btn_Confirm->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmResults); }
    RefreshResult();
    if (Bridge.IsValid()) { Bridge->ApplyButtonSounds(*this); }
}

void UTrainingResultsWidget::RefreshResult()
{
    if (Txt_ResultTitle)
    {
        Txt_ResultTitle->SetText(Result.bCompleted
            ? NSLOCTEXT("TrainingResults", "Completed", "SESSION COMPLETE")
            : NSLOCTEXT("TrainingResults", "Stopped", "SESSION ENDED"));
    }
    if (Txt_ResultMode)
    {
        Txt_ResultMode->SetText(Result.Mode == ETrainingMode::TargetAcquisition
            ? NSLOCTEXT("TrainingResults", "Target", "360 Target Acquisition")
            : NSLOCTEXT("TrainingResults", "Combat", "360 Combat Response"));
    }
    if (Txt_ResultDuration)
    {
        Txt_ResultDuration->SetText(FText::FromString(FString::Printf(TEXT("%.1f SECONDS"), Result.ElapsedSeconds)));
    }
    if (Txt_ResultKills) { Txt_ResultKills->SetText(FText::AsNumber(Result.Stats.Kills)); }
    if (Txt_ResultAccuracy)
    {
        Txt_ResultAccuracy->SetText(FText::FromString(FString::Printf(TEXT("%.1f%%"), Result.Stats.GetAccuracyPercent())));
    }
    if (Txt_ResultShots)
    {
        Txt_ResultShots->SetText(FText::Format(NSLOCTEXT("TrainingResults", "ShotDetail", "HIT SHOTS  {0} / {1}"),
            FText::AsNumber(Result.Stats.ShotsHit), FText::AsNumber(Result.Stats.ShotsFired)));
    }
}

void UTrainingResultsWidget::ConfirmResults()
{
    if (Bridge.IsValid()) { Bridge->DismissResults(*this); }
    else { DeactivateWidget(); }
}

bool UTrainingResultsWidget::NativeOnHandleBackAction()
{
    ConfirmResults();
    return true;
}

UWidget* UTrainingResultsWidget::NativeGetDesiredFocusTarget() const
{
    return Btn_Confirm ? Btn_Confirm.Get() : Super::NativeGetDesiredFocusTarget();
}

void UTrainingResultsWidget::DetachBridge()
{
    Bridge.Reset();
}

void UTrainingResultsWidget::NativeDestruct()
{
    if (Btn_Confirm) { Btn_Confirm->OnClicked.RemoveDynamic(this, &ThisClass::ConfirmResults); }
    if (Bridge.IsValid()) { Bridge->NotifyResultsRemoved(*this); }
    Bridge.Reset();
    Super::NativeDestruct();
}
