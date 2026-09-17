#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Training/TrainingTypes.h"
#include "TrainingSessionHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;

UCLASS(Blueprintable)
class FPS_API UTrainingSessionHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UTrainingSessionHUDWidget(const FObjectInitializer& ObjectInitializer);
    void StartSession(ETrainingMode Mode, double StartServerSeconds, float DurationSeconds);
    void PresentHit(int32 TotalKills, bool bKilled);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|HUD|Kill Feedback", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
    float KillFadeInSeconds = 0.10f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|HUD|Kill Feedback", meta = (ClampMin = "0.0", ClampMax = "5.0", Units = "s"))
    float KillHoldSeconds = 0.55f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|HUD|Kill Feedback", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
    float KillFadeOutSeconds = 0.35f;

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeDestruct() override;

private:
    void RefreshClock();
    void UpdateKillFeedback(float DeltaSeconds);
    void ApplyKillFeedbackAppearance();
    void ResetKillFeedback();

    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UWidget> SessionInfoContent;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Mode;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Timer;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_TimerLabel;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Kills;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_KillLabel;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UWidget> KillCounterContent;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> Bar_TimeRemaining;

    double StartServerSeconds = 0.0;
    float ActiveDurationSeconds = 0.0f;
    float KillFeedbackAge = 0.0f;
    float KillFeedbackOpacity = 0.0f;
    float KillFadeStartOpacity = 0.0f;
    bool bKillFeedbackActive = false;
    int32 DisplayedSeconds = INDEX_NONE;
    int32 DisplayedKills = 0;
    bool bSessionActive = false;
};
