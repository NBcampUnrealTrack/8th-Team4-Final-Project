#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Training/TrainingTypes.h"
#include "TrainingSessionHUDWidget.generated.h"

class UCanvasPanel;
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
    void StartDamageOnlyOverlay();
    void PresentHit(int32 TotalKills, float Damage, bool bKilled, const FVector& WorldPosition);
    void PresentDamage(float Damage, bool bKilled, const FVector& WorldPosition);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|HUD|Kill Feedback", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
    float KillFadeInSeconds = 0.10f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|HUD|Kill Feedback", meta = (ClampMin = "0.0", ClampMax = "5.0", Units = "s"))
    float KillHoldSeconds = 0.55f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|HUD|Kill Feedback", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
    float KillFadeOutSeconds = 0.35f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|HUD")
    FVector DamageNumberWorldOffset = FVector(0.0, 0.0, 110.0);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|HUD", meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float DamageNumberLifetime = 0.85f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|HUD", meta = (ClampMin = "1", ClampMax = "64"))
    int32 MaxVisibleDamageNumbers = 16;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativePreConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeDestruct() override;

private:
    void RefreshClock();
    void ClearDamageNumbers();
    void UpdateKillFeedback(float DeltaSeconds);
    void ApplyKillFeedbackAppearance();
    void ResetKillFeedback();

    UPROPERTY(Transient) TObjectPtr<UWidget> SessionInfoContent;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Mode;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Timer;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_TimerLabel;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Kills;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_KillLabel;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UWidget> KillCounterContent;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> Bar_TimeRemaining;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> Canvas_Damage;

    struct FDamageNumber
    {
        TWeakObjectPtr<UTextBlock> Text;
        FVector WorldPosition = FVector::ZeroVector;
        float Age = 0.0f;
        float HorizontalOffset = 0.0f;
    };
    TArray<FDamageNumber> DamageNumbers;
    ETrainingMode ActiveMode = ETrainingMode::TargetAcquisition;
    double StartServerSeconds = 0.0;
    float ActiveDurationSeconds = TrainingRules::TargetDurationSeconds;
    float KillFeedbackAge = 0.0f;
    float KillFeedbackOpacity = 0.0f;
    float KillFadeStartOpacity = 0.0f;
    bool bKillFeedbackActive = false;
    int32 DisplayedSeconds = INDEX_NONE;
    int32 DisplayedKills = 0;
    int32 DamageSequence = 0;
    bool bSessionActive = false;
};
