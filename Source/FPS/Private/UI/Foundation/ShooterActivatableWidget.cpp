// ShooterActivatableWidget.cpp
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Input/UIActionBindingHandle.h"

TOptional<FUIInputConfig> UShooterActivatableWidget::GetDesiredInputConfig() const{
	switch (InputConfig)
	{
	case EShooterWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture);
		
	case EShooterWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
		
	case EShooterWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
		
	case EShooterWidgetInputMode::GameAndMenu_LockLook:
		{
			FUIInputConfig Config(ECommonInputMode::All, EMouseCaptureMode::NoCapture);
			Config.bIgnoreMoveInput = false;   // 이동 유지
			Config.bIgnoreLookInput = true;    // 시점 차단
			return Config;
		}
		
	case EShooterWidgetInputMode::Default:
	default:
		return Super::GetDesiredInputConfig();
	}
}
