#include "Training/UI/TrainingStartFeedbackWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Training/TrainingTypes.h"
#include "TrainingWidgetLayout.h"

#define LOCTEXT_NAMESPACE "TrainingStartFeedback"

UTrainingStartFeedbackWidget::UTrainingStartFeedbackWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
    CountdownText = LOCTEXT("CountdownLive", "{Seconds}초 뒤에 시작합니다");
    StartText = LOCTEXT("Start", "START");
    MessageFont = TrainingWidgetLayout::MakeFont(22);
}

TSharedRef<SWidget> UTrainingStartFeedbackWidget::RebuildWidget()
{
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
    }
    if (!WidgetTree->RootWidget) { TrainingWidgetLayout::BuildCountdown(*WidgetTree); }
    TrainingWidgetLayout::RepairTextFonts(*WidgetTree);
    FeedbackContent = WidgetTree->FindWidget(TEXT("FeedbackContent"));
    Txt_FeedbackLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_FeedbackLabel")));
    Txt_FeedbackMessage = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_FeedbackMessage")));
    return Super::RebuildWidget();
}

void UTrainingStartFeedbackWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyStyle();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    if (IsDesignTime() && Txt_FeedbackMessage)
    {
        Txt_FeedbackMessage->SetText(FText::Format(LOCTEXT("CountdownPreview", "{0}초 뒤에 시작합니다"), FText::AsNumber(3)));
    }
}

void UTrainingStartFeedbackWidget::ApplyStyle()
{
    TrainingWidgetLayout::RepairFont(MessageFont);
    if (FeedbackContent)
    {
        if (UCanvasPanelSlot* ContentSlot = Cast<UCanvasPanelSlot>(FeedbackContent->Slot))
        {
            ContentSlot->SetPosition(OffsetFromAim);
        }
    }
    if (Txt_FeedbackLabel)
    {
        FSlateFontInfo LabelFont = MessageFont;
        LabelFont.Size = 11;
        Txt_FeedbackLabel->SetFont(LabelFont);
        Txt_FeedbackLabel->SetText(LOCTEXT("Label", "TRAINING"));
        Txt_FeedbackLabel->SetColorAndOpacity(FSlateColor(AccentColor));
        Txt_FeedbackLabel->SetShadowOffset(FVector2D(1.0, 1.0));
        Txt_FeedbackLabel->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
    }
    if (Txt_FeedbackMessage)
    {
        Txt_FeedbackMessage->SetFont(MessageFont);
        Txt_FeedbackMessage->SetColorAndOpacity(FSlateColor(MessageColor));
        Txt_FeedbackMessage->SetShadowOffset(FVector2D(1.0, 1.0));
        Txt_FeedbackMessage->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
    }
}

void UTrainingStartFeedbackWidget::ShowCountdown(const float ElapsedServerSeconds)
{
    bCountdownPulse = true;
    LastCountdownDigit = INDEX_NONE;
    ShowPulse(CountdownText, TrainingRules::CountdownSeconds, ElapsedServerSeconds);
}

void UTrainingStartFeedbackWidget::ShowStarted(const float ElapsedServerSeconds)
{
    bCountdownPulse = false;
    ShowPulse(StartText, FMath::Max(0.5f, StartDisplaySeconds), ElapsedServerSeconds);
}

void UTrainingStartFeedbackWidget::ShowPulse(const FText& Message,
    const float Duration, const float InitialElapsed)
{
    ApplyStyle();
    if (!GetWorld() || !FeedbackContent || !Txt_FeedbackMessage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] Feedback widget requires FeedbackContent and Txt_FeedbackMessage."));
        return;
    }
    LocalPulseStartSeconds = GetWorld()->GetTimeSeconds();
    InitialElapsedSeconds = FMath::Max(0.0f, InitialElapsed);
    PulseDurationSeconds = Duration;
    Txt_FeedbackMessage->SetText(Message);
    SetVisibility(ESlateVisibility::HitTestInvisible);
    GetWorld()->GetTimerManager().SetTimer(PulseUpdateTimer, this, &ThisClass::UpdatePulse,
        1.0f / 60.0f, true);
    UpdatePulse();
}

void UTrainingStartFeedbackWidget::UpdatePulse()
{
    const float Elapsed = InitialElapsedSeconds
        + static_cast<float>(GetWorld()->GetTimeSeconds() - LocalPulseStartSeconds);
    if (Elapsed >= PulseDurationSeconds)
    {
        ClearFeedback();
        return;
    }
    if (bCountdownPulse && Txt_FeedbackMessage)
    {
        const int32 Digit = TrainingRules::RemainingWholeSeconds(TrainingRules::CountdownSeconds, Elapsed);
        if (Digit != LastCountdownDigit)
        {
            LastCountdownDigit = Digit;
            // Old child WBPs may have serialized the fixed "3초..." text. Use a live format
            const FText Format = CountdownText.ToString().Contains(TEXT("{Seconds}"))
                ? CountdownText : LOCTEXT("CountdownLive", "{Seconds}초 뒤에 시작합니다");
            FFormatNamedArguments Args;
            Args.Add(TEXT("Seconds"), FText::AsNumber(Digit));
            Txt_FeedbackMessage->SetText(FText::Format(Format, Args));
        }
    }
    const float Opacity = TrainingRules::FeedbackOpacity(
        Elapsed, PulseDurationSeconds, FadeInSeconds, FadeOutSeconds);
    FeedbackContent->SetRenderOpacity(Opacity);
    const bool bFadingIn = Elapsed < FMath::Min(FadeInSeconds, PulseDurationSeconds * 0.5f);
    const double SlideX = (1.0 - static_cast<double>(Opacity)) * (bFadingIn ? 12.0 : -8.0);
    FeedbackContent->SetRenderTranslation(FVector2D(SlideX, 0.0));
}

void UTrainingStartFeedbackWidget::ClearFeedback()
{
    if (UWorld* ActiveWorld = GetWorld())
    {
        ActiveWorld->GetTimerManager().ClearTimer(PulseUpdateTimer);
    }
    if (FeedbackContent)
    {
        FeedbackContent->SetRenderOpacity(0.0f);
        FeedbackContent->SetRenderTranslation(FVector2D::ZeroVector);
    }
    SetVisibility(ESlateVisibility::Collapsed);
}

void UTrainingStartFeedbackWidget::NativeDestruct()
{
    ClearFeedback();
    Super::NativeDestruct();
}

#undef LOCTEXT_NAMESPACE
