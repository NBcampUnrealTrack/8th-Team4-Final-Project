// ShooterRoomMenuScreen.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterRoomMenuScreen.generated.h"

class UShooterButtonBase;
class UWidget;

DECLARE_MULTICAST_DELEGATE(FOnRoomMenuAction);

/**
 * 방에서 ESC 로 여는 메뉴
 */
UCLASS(Abstract)
class FPS_API UShooterRoomMenuScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	FOnRoomMenuAction OnCloseRequested;
	FOnRoomMenuAction OnSettingsRequested;
	FOnRoomMenuAction OnLeaveRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	void HandleSettingsClicked();
	void HandleLeaveClicked();
	void HandleCloseClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Settings;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Leave;
	// 없으면 ESC 로만 닫는다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UShooterButtonBase> Btn_Close;
};
