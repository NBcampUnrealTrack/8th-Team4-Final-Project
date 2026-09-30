#include "Training/UI/TrainingStartFeedbackWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Training/TrainingSettings.h"
#include "Training/TrainingTypes.h"
#include "TrainingWidgetStyle.h"

#define LOCTEXT_NAMESPACE "TrainingStartFeedback"

UTrainingStartFeedbackWidget::UTrainingStartFeedbackWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
    CountdownText = LOCTEXT("CountdownLive", "{Seconds}초 뒤에 시작합니다");
    StartText = LOCTEXT("Start", "START");
    MessageFont = TrainingWidgetStyle::MakeFont(22);
}

void UTrainingStartFeedbackWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyStyle();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    if (IsDesignTime() && Txt_FeedbackMessage)
    {
        Txt_FeedbackMessage->SetText(FText::Format(LOCTEXT("CountdownPreview", "{0}초 뒤에 시작합니다"), FText::AsNumber(GetDefault<UTrainingSettings>()->GetCountdownSeconds())));
    }
}

void UTrainingStartFeedbackWidget::ApplyStyle()
{
    TrainingWidgetStyle::RepairFont(MessageFont);
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

void UTrainingStartFeedbackWidget::ShowCountdown(const float ElapsedSeconds)
{
    bCountdownPulse = true;
    LastCountdownDigit = INDEX_NONE;
    ShowPulse(CountdownText, GetDefault<UTrainingSettings>()->GetCountdownSeconds(), ElapsedSeconds);
}

void UTrainingStartFeedbackWidget::ShowStarted(const float ElapsedSeconds)
{
    bCountdownPulse = false;
    ShowPulse(StartText, FMath::Max(0.5f, StartDisplaySeconds), ElapsedSeconds);
}

void UTrainingStartFeedbackWidget::ShowPulse(const FText& Message,
    const float Duration, const float InitialElapsed)
{
    ClearFeedback();
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
    FTimerManagerTimerParameters TimerParameters;
    TimerParameters.bLoop = true;
    TimerParameters.bMaxOncePerFrame = true;
    GetWorld()->GetTimerManager().SetTimer(PulseUpdateTimer, this, &ThisClass::UpdatePulse,
        TrainingRules::PresentationUpdateIntervalSeconds, TimerParameters);
    UpdatePulse();
    if (bCountdownPulse) { RefreshCountdown(); }
}

void UTrainingStartFeedbackWidget::RefreshCountdown()
{
    const double Elapsed = InitialElapsedSeconds + GetWorld()->GetTimeSeconds() - LocalPulseStartSeconds;
    const double Remaining = FMath::Max(0.0, PulseDurationSeconds - Elapsed);
    if (Remaining <= 0.0) { return; }
    const int32 Digit = FMath::CeilToInt(Remaining);
    if (Txt_FeedbackMessage && Digit != LastCountdownDigit)
    {
        LastCountdownDigit = Digit;
        const FText Format = CountdownText.ToString().Contains(TEXT("{Seconds}"))
            ? CountdownText : LOCTEXT("CountdownLive", "{Seconds}초 뒤에 시작합니다");
        FFormatNamedArguments Args;
        Args.Add(TEXT("Seconds"), FText::AsNumber(Digit));
        Txt_FeedbackMessage->SetText(FText::Format(Format, Args));
    }
    GetWorld()->GetTimerManager().SetTimer(CountdownTimer, this, &ThisClass::RefreshCountdown,
        static_cast<float>(FMath::Max(0.01, Remaining - (Digit - 1))), false);
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
        ActiveWorld->GetTimerManager().ClearTimer(CountdownTimer);
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
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }
    PulseUpdateTimer.Invalidate();
    CountdownTimer.Invalidate();
    ClearFeedback();
    Super::NativeDestruct();
}

#undef LOCTEXT_NAMESPACE
