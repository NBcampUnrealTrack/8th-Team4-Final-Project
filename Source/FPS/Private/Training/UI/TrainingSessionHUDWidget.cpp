#include "Training/UI/TrainingSessionHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Training/TrainingSettings.h"
#include "Training/TrainingModeDefinition.h"

UTrainingSessionHUDWidget::UTrainingSessionHUDWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
}

void UTrainingSessionHUDWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyKillFeedbackAppearance();
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTrainingSessionHUDWidget::StartSession(const ETrainingMode Mode,
    const double InStartSeconds, const float DurationSeconds)
{
    GetWorld()->GetTimerManager().ClearTimer(ClockTimer);
    StartSeconds = InStartSeconds;
    ActiveDurationSeconds = FMath::Max(0.0f, DurationSeconds);
    bSessionActive = true;
    bCQBSession = false;
    CQBTotalTroopers = 0;
    if (CQBProgressContent) { CQBProgressContent->SetVisibility(ESlateVisibility::Collapsed); }
    if (SessionInfoContent) { SessionInfoContent->SetVisibility(ESlateVisibility::HitTestInvisible); }
    if (Txt_Mode) { Txt_Mode->SetVisibility(ESlateVisibility::HitTestInvisible); }
    if (Txt_Timer) { Txt_Timer->SetVisibility(ESlateVisibility::HitTestInvisible); }
    if (Txt_TimerLabel) { Txt_TimerLabel->SetVisibility(ESlateVisibility::HitTestInvisible); }
    DisplayedSeconds = INDEX_NONE;
    DisplayedKills = 0;
    ResetKillFeedback();
    if (Txt_Kills)
    {
        Txt_Kills->SetText(FText::GetEmpty());
        Txt_Kills->SetRenderScale(FVector2D(1.0, 1.0));
    }
    if (Txt_Mode)
    {
        Txt_Mode->SetText(TrainingModeDefinitions::GetHUDLabel(Mode));
    }
    if (Txt_TimerLabel)
    {
        Txt_TimerLabel->SetText(ActiveDurationSeconds > 0.0f
            ? NSLOCTEXT("TrainingHUD", "Remaining", "SECONDS REMAINING")
            : NSLOCTEXT("TrainingHUD", "Elapsed", "ELAPSED  /  MANUAL END"));
    }
    if (Bar_TimeRemaining)
    {
        Bar_TimeRemaining->SetVisibility(ActiveDurationSeconds > 0.0f
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    RefreshClock();
}

void UTrainingSessionHUDWidget::StartCQBSession(const int32 TotalTroopers)
{
    GetWorld()->GetTimerManager().ClearTimer(ClockTimer);
    bSessionActive = true;
    bCQBSession = true;
    CQBTotalTroopers = FMath::Max(0, TotalTroopers);
    DisplayedKills = 0;
    DisplayedSeconds = INDEX_NONE;
    ResetKillFeedback();

    if (SessionInfoContent) SessionInfoContent->SetVisibility(ESlateVisibility::Collapsed);
    if (Txt_Mode) Txt_Mode->SetVisibility(ESlateVisibility::Collapsed);
    if (Txt_Timer) Txt_Timer->SetVisibility(ESlateVisibility::Collapsed);
    if (Txt_TimerLabel) Txt_TimerLabel->SetVisibility(ESlateVisibility::Collapsed);
    if (Bar_TimeRemaining) Bar_TimeRemaining->SetVisibility(ESlateVisibility::Collapsed);
    if (CQBProgressContent) CQBProgressContent->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (Txt_ProgressLabel)
    {
        Txt_ProgressLabel->SetText(NSLOCTEXT("TrainingHUD", "CQBCleared", "TARGETS CLEARED"));
    }
    if (Txt_Progress)
    {
        Txt_Progress->SetText(FText::FromString(FString::Printf(TEXT("0/%d"), CQBTotalTroopers)));
    }
    if (Txt_Kills) Txt_Kills->SetText(FText::GetEmpty());
}

void UTrainingSessionHUDWidget::PresentCQBKill(const int32 CurrentKills, const int32 TotalTroopers)
{
    if (!bSessionActive || !bCQBSession) return;

    CQBTotalTroopers = FMath::Max(0, TotalTroopers);
    PresentHit(CurrentKills, true);
    if (Txt_Progress)
    {
        Txt_Progress->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"),
            FMath::Clamp(CurrentKills, 0, CQBTotalTroopers), CQBTotalTroopers)));
    }
}

void UTrainingSessionHUDWidget::FinishCQBSession()
{
    if (Txt_Progress && CQBTotalTroopers > 0)
    {
        Txt_Progress->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), CQBTotalTroopers, CQBTotalTroopers)));
    }
}

void UTrainingSessionHUDWidget::RefreshClock()
{
    if (!bSessionActive || bCQBSession || !GetWorld()) { return; }
    const double Elapsed = FMath::Max(0.0, GetWorld()->GetTimeSeconds() - StartSeconds);
    const int32 Seconds = ActiveDurationSeconds > 0.0f
        ? FMath::CeilToInt(FMath::Max(0.0, ActiveDurationSeconds - Elapsed)) : FMath::FloorToInt(Elapsed);
    if (Seconds != DisplayedSeconds)
    {
        DisplayedSeconds = Seconds;
        if (Txt_Timer)
        {
            Txt_Timer->SetText(FText::FromString(ActiveDurationSeconds > 0.0f
                ? FString::Printf(TEXT("%02d"), Seconds)
                : FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60)));
            const float UrgentThreshold = FMath::Max(0.0f, GetDefault<UTrainingSettings>()->UrgentTimerThresholdSeconds);
            Txt_Timer->SetColorAndOpacity(FSlateColor(ActiveDurationSeconds > 0.0f && Seconds <= FMath::CeilToInt(UrgentThreshold)
                ? FLinearColor(1.0f, 0.43f, 0.25f) : FLinearColor(0.93f, 0.97f, 1.0f)));
        }
    }
    if (Bar_TimeRemaining && ActiveDurationSeconds > 0.0f)
    {
        Bar_TimeRemaining->SetPercent(static_cast<float>(FMath::Clamp(1.0 - Elapsed / ActiveDurationSeconds, 0.0, 1.0)));
    }
    if (ActiveDurationSeconds <= 0.0f || Elapsed < ActiveDurationSeconds)
    {
        const double NextBoundary = ActiveDurationSeconds > 0.0f
            ? ActiveDurationSeconds - Elapsed - (Seconds - 1)
            : (Seconds + 1) - Elapsed;
        GetWorld()->GetTimerManager().SetTimer(ClockTimer, this, &ThisClass::RefreshClock,
            static_cast<float>(FMath::Max(0.01, NextBoundary)), false);
    }
}

void UTrainingSessionHUDWidget::PresentHit(const int32 TotalKills, const bool bKilled)
{
    if (!bSessionActive)
    {
        return;
    }

    if (bKilled && TotalKills > DisplayedKills)
    {
        DisplayedKills = TotalKills;
        KillFadeStartOpacity = bKillFeedbackActive ? KillFeedbackOpacity : 0.0f;
        KillFeedbackStartSeconds = 0.0;
        bKillFeedbackActive = true;
        if (Txt_Kills)
        {
            Txt_Kills->SetText(FText::FromString(FString::Printf(TEXT("+%d"), TotalKills)));
        }
        StartKillFeedbackAnimation();
    }
}

void UTrainingSessionHUDWidget::StartKillFeedbackAnimation()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        ResetKillFeedback();
        return;
    }

    World->GetTimerManager().ClearTimer(KillFeedbackTimer);
    KillFeedbackStartSeconds = World->GetTimeSeconds();
    UpdateKillFeedback();
    if (bKillFeedbackActive)
    {
        FTimerManagerTimerParameters TimerParameters;
        TimerParameters.bLoop = true;
        TimerParameters.bMaxOncePerFrame = true;
        World->GetTimerManager().SetTimer(
            KillFeedbackTimer, this, &ThisClass::UpdateKillFeedback,
            TrainingRules::PresentationUpdateIntervalSeconds, TimerParameters);
    }
}

void UTrainingSessionHUDWidget::UpdateKillFeedback()
{
    if (!bKillFeedbackActive)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        ResetKillFeedback();
        return;
    }

    const float KillFeedbackAge = static_cast<float>(
        FMath::Max(0.0, World->GetTimeSeconds() - KillFeedbackStartSeconds));
    const float FadeIn = FMath::Max(0.0f, KillFadeInSeconds);
    const float Hold = FMath::Max(0.0f, KillHoldSeconds);
    const float FadeOut = FMath::Max(0.0f, KillFadeOutSeconds);
    const float FadeOutStart = FadeIn + Hold;
    if (KillFeedbackAge >= FadeOutStart + FadeOut)
    {
        ResetKillFeedback();
        return;
    }
    const auto Smooth = [](const float Alpha)
    {
        const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    };
    if (FadeIn > KINDA_SMALL_NUMBER && KillFeedbackAge < FadeIn)
    {
        KillFeedbackOpacity = FMath::Lerp(
            KillFadeStartOpacity, 1.0f, Smooth(KillFeedbackAge / FadeIn));
    }
    else if (KillFeedbackAge < FadeOutStart)
    {
        KillFeedbackOpacity = 1.0f;
    }
    else
    {
        KillFeedbackOpacity = FadeOut > KINDA_SMALL_NUMBER
            ? 1.0f - Smooth((KillFeedbackAge - FadeOutStart) / FadeOut) : 0.0f;
    }
    ApplyKillFeedbackAppearance();
}

void UTrainingSessionHUDWidget::ApplyKillFeedbackAppearance()
{
    const ESlateVisibility CounterVisibility = bKillFeedbackActive
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
    if (KillCounterContent)
    {
        KillCounterContent->SetRenderOpacity(KillFeedbackOpacity);
        KillCounterContent->SetVisibility(CounterVisibility);
    }
    else
    {
        for (UTextBlock* Label : {Txt_Kills.Get(), Txt_KillLabel.Get()})
        {
            if (Label)
            {
                Label->SetRenderOpacity(KillFeedbackOpacity);
                Label->SetVisibility(CounterVisibility);
            }
        }
    }
}

void UTrainingSessionHUDWidget::ResetKillFeedback()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(KillFeedbackTimer);
    }
    bKillFeedbackActive = false;
    KillFeedbackStartSeconds = 0.0;
    KillFeedbackOpacity = 0.0f;
    KillFadeStartOpacity = 0.0f;
    ApplyKillFeedbackAppearance();
}

void UTrainingSessionHUDWidget::StopSession()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ClockTimer);
        World->GetTimerManager().ClearTimer(KillFeedbackTimer);
    }
    bSessionActive = false;
    bCQBSession = false;
    CQBTotalTroopers = 0;
    StartSeconds = 0.0;
    ActiveDurationSeconds = 0.0f;
    ResetKillFeedback();
}

void UTrainingSessionHUDWidget::NativeDestruct()
{
    StopSession();
    Super::NativeDestruct();
}
