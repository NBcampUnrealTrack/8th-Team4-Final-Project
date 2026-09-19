#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Training/TrainingTypes.h"
#include "CQBTrainingDialogWidget.generated.h"

class UButton;
class UTextBlock;
class UTrainingPlayerComponent;

UCLASS(Abstract, Blueprintable)
class FPS_API UCQBTrainingDialogWidget : public UShooterActivatableWidget
{
    GENERATED_BODY()

public:
    UCQBTrainingDialogWidget();

    void InitializeDialog(UTrainingPlayerComponent& InBridge, ECQBTrainingDialogKind InKind, int32 InRequestId);
    void DetachRequest();
    void ReceiveRequestFailure(const FText& Failure);
    int32 GetRequestId() const { return RequestId; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
    UFUNCTION()
    void HandlePrimary();

    UFUNCTION()
    void HandleSecondary();

    UFUNCTION()
    void HandleClose();

    void RefreshText();
    void SetPending(bool bPending);

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Primary;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Secondary;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Close;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Title;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Prompt;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Primary;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Secondary;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Error;

    TWeakObjectPtr<UTrainingPlayerComponent> Bridge;
    ECQBTrainingDialogKind DialogKind = ECQBTrainingDialogKind::Start;
    int32 RequestId = 0;
    bool bRequestPending = false;
};
