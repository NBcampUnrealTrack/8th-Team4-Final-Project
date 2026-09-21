#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Training/TrainingTypes.h"
#include "Training/TrainingLoadoutSnapshot.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"
#include "TrainingDialogWidget.generated.h"

class UTrainingPlayerComponent;
class UButton;
class UComboBoxString;
class USpinBox;
class UTextBlock;
class UImage;
class UWidgetSwitcher;
class UTrainingAugmentSelectionWidget;

UCLASS(Abstract, Blueprintable)
class FPS_API UTrainingDialogWidget : public UShooterActivatableWidget
{
    GENERATED_BODY()

public:
    UTrainingDialogWidget();
    void InitializeDialog(UTrainingPlayerComponent& InBridge, ETrainingDialogKind InKind, int32 InRequestId);
    void DetachRequest();
    void ReceiveRequestFailure(const FText& Failure);
    void SetAppliedAugments(const TArray<FAugmentInventoryEntry>& Entries);
    void SetAppliedLoadout(const FTrainingLoadoutSnapshot& Snapshot);
    void ReceiveCombatClassResult(bool bSuccess, const FTrainingLoadoutSnapshot& Snapshot, const FText& Failure);
    void ReceiveAugmentResult(bool bSuccess, const FTrainingLoadoutSnapshot& Snapshot, const FText& Failure);

    UFUNCTION(BlueprintCallable, Category = "Training")
    void OpenAugmentSelection();
    int32 GetRequestId() const { return RequestId; }

    UFUNCTION(BlueprintCallable, Category = "Training")
    void ConfirmDialog();

    UFUNCTION(BlueprintCallable, Category = "Training")
    void EnterArenaDestination();

    UFUNCTION(BlueprintCallable, Category = "Training")
    void EnterCQBDestination();

    UFUNCTION(BlueprintCallable, Category = "Training")
    void StartSelectedMode(ETrainingMode Mode, ETrainingDistancePreset Preset, float CustomMeters = 20.0f);

    UFUNCTION(BlueprintCallable, Category = "Training")
    void CancelDialog();

    UFUNCTION(BlueprintCallable, Category = "Training")
    void ReturnToStartPosition();

    UFUNCTION(BlueprintPure, Category = "Training")
    FText GetPromptText() const;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    ETrainingDialogKind DialogKind = ETrainingDialogKind::EnterArena;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    bool bRequestPending = false;

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
    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Primary;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_EnterArena;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_EnterCQB;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_ReturnStart;

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

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Augments;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_AugmentSummary;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UComboBoxString> Combo_Class;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_ClassHint;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UWidgetSwitcher> Switch_TrainingPages;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTrainingAugmentSelectionWidget> AugmentSelection;

    UPROPERTY(Transient)
    TArray<FAugmentInventoryEntry> AppliedAugments;
    bool bChoosingAugments = false;
    void HandleAugmentApply(const TArray<FPrimaryAssetId>& Selection);
    void HandleAugmentBack();
    void RefreshAugmentSummary();

    void BuildClassOptions();
    void RefreshClassControls();

    UFUNCTION()
    void HandleGeneratedClassSelection(FString SelectedItem, ESelectInfo::Type SelectionType);

    TArray<EDominationCombatClass> ClassOptions;
    EDominationCombatClass AppliedCombatClass = EDominationCombatClass::Unselected;
    EDominationCombatClass PendingCombatClass = EDominationCombatClass::Unselected;
    FText ClassFeedback;
    bool bUpdatingClassControl = false;

    void BindControls();
    void UnbindControls();
    void RefreshControls();
    void SelectDistance(ETrainingDistancePreset NewPreset);

    UFUNCTION()
    void HandleGeneratedPrimary();

    UFUNCTION()
    void HandleGeneratedTarget();

    UFUNCTION()
    void HandleGeneratedCombat();

    UFUNCTION()
    void HandleGenerated10();

    UFUNCTION()
    void HandleGenerated20();

    UFUNCTION()
    void HandleGenerated30();

    UFUNCTION()
    void HandleGeneratedCustom();

    UFUNCTION()
    void HandleGeneratedCustomValue(float NewValue);

    ETrainingMode SelectedMode = ETrainingMode::TargetAcquisition;
    ETrainingDistancePreset SelectedPreset = ETrainingDistancePreset::TwentyMeters;
    float SelectedCustomMeters = 0.0f;

    void SetRequestPending(bool bPending);
    TWeakObjectPtr<UTrainingPlayerComponent> Bridge;
    int32 RequestId = 0;
};
