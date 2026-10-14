#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "TimerManager.h"
#include "TrainingStartFeedbackWidget.generated.h"

class UTextBlock;
class UWidget;

UCLASS(Blueprintable)
class FPS_API UTrainingStartFeedbackWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UTrainingStartFeedbackWidget(const FObjectInitializer& ObjectInitializer);
    void ShowCountdown(float ElapsedSeconds);
    void ShowStarted(float ElapsedSeconds);
    void ClearFeedback();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Feedback")
    FVector2D OffsetFromAim = FVector2D(72.0, -12.0);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Feedback")
    FText CountdownText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Feedback")
    FText StartText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Feedback")
    FSlateFontInfo MessageFont;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Feedback")
    FLinearColor MessageColor = FLinearColor(0.91f, 0.97f, 1.0f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Feedback")
    FLinearColor AccentColor = FLinearColor(0.19f, 0.80f, 0.93f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Feedback", meta = (ClampMin = "0.01", Units = "s"))
    float FadeInSeconds = 0.20f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Feedback", meta = (ClampMin = "0.01", Units = "s"))
    float FadeOutSeconds = 0.30f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Feedback", meta = (ClampMin = "0.5", Units = "s"))
    float StartDisplaySeconds = 1.6f;

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> FeedbackContent;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_FeedbackLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_FeedbackMessage;

private:
    void ShowPulse(const FText& Message, float Duration, float InitialElapsed);
    void UpdatePulse();
    void RefreshCountdown();
    void ApplyStyle();
    bool bCountdownPulse = false;
    int32 LastCountdownDigit = INDEX_NONE;
    double LocalPulseStartSeconds = 0.0;
    float InitialElapsedSeconds = 0.0f;
    float PulseDurationSeconds = 0.0f;
    FTimerHandle PulseUpdateTimer;
    FTimerHandle CountdownTimer;
};
