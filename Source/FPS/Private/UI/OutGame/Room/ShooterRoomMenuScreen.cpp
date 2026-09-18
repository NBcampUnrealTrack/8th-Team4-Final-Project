// ShooterRoomMenuScreen.cpp
#include "UI/OutGame/Room/ShooterRoomMenuScreen.h"

#include "UI/Foundation/ShooterButtonBase.h"

void UShooterRoomMenuScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Btn_Settings->OnClicked().AddUObject(this, &ThisClass::HandleSettingsClicked);
	Btn_Leave->OnClicked().AddUObject(this, &ThisClass::HandleLeaveClicked);

	if (Btn_Close)
	{
		Btn_Close->OnClicked().AddUObject(this, &ThisClass::HandleCloseClicked);
	}
}

bool UShooterRoomMenuScreen::NativeOnHandleBackAction()
{
	OnCloseRequested.Broadcast();
	return true;
}

UWidget* UShooterRoomMenuScreen::NativeGetDesiredFocusTarget() const
{
	return Btn_Settings;
}

void UShooterRoomMenuScreen::HandleSettingsClicked()
{
	OnSettingsRequested.Broadcast();
}

void UShooterRoomMenuScreen::HandleLeaveClicked()
{
	OnLeaveRequested.Broadcast();
}

void UShooterRoomMenuScreen::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}
