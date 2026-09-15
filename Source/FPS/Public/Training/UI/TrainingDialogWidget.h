#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Training/TrainingTypes.h"
#include "Styling/SlateTypes.h"
#include "TrainingDialogWidget.generated.h"

class UTrainingPlayerComponent;
class UButton;
class USpinBox;
class UTextBlock;
class UImage;

// WBP supplies all visuals. These functions are the only button-to-gameplay contract.
UCLASS(Abstract, Blueprintable)
class FPS_API UTrainingDialogWidget : public UShooterActivatableWidget
{
    GENERATED_BODY()

public:
    UTrainingDialogWidget();
    void InitializeDialog(UTrainingPlayerComponent& InBridge, ETrainingDialogKind InKind, int32 InRequestId);
    void DetachRequest();
    void ReceiveRequestFailure(const FText& Failure);
    int32 GetRequestId() const { return RequestId; }

    UFUNCTION(BlueprintCallable, Category = "Training")
    void ConfirmDialog();

    UFUNCTION(BlueprintCallable, Category = "Training")
    void StartSelectedMode(ETrainingMode Mode, ETrainingDistancePreset Preset, float CustomMeters = 20.0f);

    UFUNCTION(BlueprintCallable, Category = "Training")
    void CancelDialog();

    UFUNCTION(BlueprintPure, Category = "Training")
    FText GetPromptText() const;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    ETrainingDialogKind DialogKind = ETrainingDialogKind::EnterArena;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    bool bRequestPending = false;

    // Opt-in: existing hand-authored WBP event graphs continue to work unchanged.
    UPROPERTY(EditDefaultsOnly, Category = "Training|Generated UI")
    bool bAutoBindGeneratedUI = false;

    UPROPERTY(EditDefaultsOnly, Category = "Training|Generated UI")
    ETrainingDialogKind GeneratedDialogKind = ETrainingDialogKind::EnterArena;

    UPROPERTY(EditDefaultsOnly, Category = "Training|Generated UI")
    FText PrimaryActionText;

    UPROPERTY(EditDefaultsOnly, Category = "Training|Generated UI")
    FButtonStyle ChoiceNormalStyle;

    UPROPERTY(EditDefaultsOnly, Category = "Training|Generated UI")
    FButtonStyle ChoiceSelectedStyle;

protected:
    virtual void NativeConstruct() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual void NativeDestruct() override;
    virtual bool NativeOnHandleBackAction() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Training")
    void OnDialogPrepared();

    UFUNCTION(BlueprintImplementableEvent, Category = "Training")
    void OnRequestPendingChanged(bool bPending);

    UFUNCTION(BlueprintImplementableEvent, Category = "Training")
    void OnRequestFailed(const FText& Failure);

private:
    // Optional names are the generator contract, not a runtime-created layout.
    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Primary;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Cancel;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Close;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_ModeTarget;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_ModeCombat;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Distance10;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Distance20;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Distance30;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_DistanceCustom;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<USpinBox> Spin_CustomMeters;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Primary;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Prompt;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Error;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Range;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_SessionHint;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UImage> Img_TargetSelected;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UImage> Img_CombatSelected;

    void BindGeneratedControls();
    void UnbindGeneratedControls();
    void RefreshGeneratedControls();
    void SelectGeneratedDistance(ETrainingDistancePreset NewPreset);

    UFUNCTION() void HandleGeneratedPrimary();
    UFUNCTION() void HandleGeneratedTarget();
    UFUNCTION() void HandleGeneratedCombat();
    UFUNCTION() void HandleGenerated10();
    UFUNCTION() void HandleGenerated20();
    UFUNCTION() void HandleGenerated30();
    UFUNCTION() void HandleGeneratedCustom();
    UFUNCTION() void HandleGeneratedCustomValue(float NewValue);

    ETrainingMode SelectedMode = ETrainingMode::TargetAcquisition;
    ETrainingDistancePreset SelectedPreset = ETrainingDistancePreset::TwentyMeters;
    float SelectedCustomMeters = 25.0f;

    void SetRequestPending(bool bPending);
    TWeakObjectPtr<UTrainingPlayerComponent> Bridge;
    int32 RequestId = 0;
};
