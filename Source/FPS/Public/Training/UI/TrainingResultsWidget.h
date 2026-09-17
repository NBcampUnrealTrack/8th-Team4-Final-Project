#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Training/TrainingTypes.h"
#include "TrainingResultsWidget.generated.h"

class UTrainingPlayerComponent;
class UTextBlock;
class UButton;

UCLASS(Blueprintable)
class FPS_API UTrainingResultsWidget : public UShooterActivatableWidget
{
    GENERATED_BODY()
public:
    UTrainingResultsWidget();
    void InitializeResults(UTrainingPlayerComponent& InBridge, const FTrainingSessionResult& InResult);
    void DetachBridge();

    UFUNCTION(BlueprintCallable, Category = "Training")
    void ConfirmResults();

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    FTrainingSessionResult Result;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual bool NativeOnHandleBackAction() override;

private:
    void RefreshResult();
    TWeakObjectPtr<UTrainingPlayerComponent> Bridge;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Confirm;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ResultTitle;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ResultMode;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ResultDuration;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ResultKills;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ResultAccuracy;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ResultShots;
};
