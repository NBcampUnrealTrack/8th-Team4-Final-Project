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
		
	case EShooterWidgetInputMode::Default:
	default:
		return Super::GetDesiredInputConfig();
	}
}
