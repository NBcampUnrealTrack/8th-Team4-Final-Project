// ShooterCreateRoomDialog.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterCreateRoomDialog.generated.h"

class UEditableTextBox;
class USessionSubSystem;
class UShooterButtonBase;
class USlider;
class UTextBlock;

/**
 * 방 만들기 창 (Modal). 방 이름·최대 인원을 받아 생성 요청
 */
UCLASS(meta = (DisableNativeTick))
class FPS_API UShooterCreateRoomDialog : public UShooterActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;

	// 문구가 비면 배경 판까지 숨기도록 WBP 가 받아 처리
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void BP_ApplyStatus(bool bHasStatus);

private:
	void HandleConfirmClicked();
	void HandleCancelClicked();

	UFUNCTION()
	void HandleMaxPlayersChanged(float NewValue);
	UFUNCTION()
	void HandleCreateRoomComplete(bool bWasSuccessful);

	USessionSubSystem* GetSessionSubsystem() const;
	FString GetLocalPlayerName() const;
	int32 GetSelectedMaxPlayers() const;
	void SetBusy(bool bNewBusy, const FText& StatusText);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> TextBox_RoomName;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> Slider_MaxPlayers;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_MaxPlayers;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Confirm;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Cancel;

	// 생성 요청 중. 입력칸·취소까지 잠금
	bool bIsBusy = false;
};
