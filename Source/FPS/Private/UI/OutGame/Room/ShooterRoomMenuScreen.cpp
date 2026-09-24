// ShooterRoomMenuScreen.cpp
#include "UI/OutGame/Room/ShooterRoomMenuScreen.h"

#include "Animation/WidgetAnimation.h"
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

	if (Anim_Exit)
	{
		ExitFinishedDelegate.BindDynamic(this, &ThisClass::HandleExitFinished);
		BindToAnimationFinished(Anim_Exit, ExitFinishedDelegate);
	}
}

void UShooterRoomMenuScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 스택 재사용 · 설정에서 복귀할 때도 불린다
	bClosing = false;
	if (Anim_Exit)
	{
		StopAnimation(Anim_Exit);
	}
	if (Anim_Enter)
	{
		PlayAnimation(Anim_Enter);
	}
}

bool UShooterRoomMenuScreen::NativeOnHandleBackAction()
{
	RequestClose();
	return true;
}

UWidget* UShooterRoomMenuScreen::NativeGetDesiredFocusTarget() const
{
	return Btn_Settings;
}

void UShooterRoomMenuScreen::RequestClose()
{
	if (bClosing)
	{
		return;
	}

	if (!Anim_Exit)
	{
		OnCloseRequested.Broadcast();
		return;
	}

	bClosing = true;
	PlayAnimation(Anim_Exit);
}

void UShooterRoomMenuScreen::HandleExitFinished()
{
	if (!bClosing)
	{
		return;
	}

	bClosing = false;
	OnCloseRequested.Broadcast();
}

void UShooterRoomMenuScreen::HandleSettingsClicked()
{
	if (bClosing)
	{
		return;
	}
	OnSettingsRequested.Broadcast();
}

void UShooterRoomMenuScreen::HandleLeaveClicked()
{
	if (bClosing)
	{
		return;
	}
	OnLeaveRequested.Broadcast();
}

void UShooterRoomMenuScreen::HandleCloseClicked()
{
	RequestClose();
}
