// ShooterButtonBase.cpp
#include "UI/Foundation/ShooterButtonBase.h"

void UShooterButtonBase::SetButtonText(const FText& InText){
	ButtonText = InText;
	
	UpdateButtonText(ButtonText);
}

void UShooterButtonBase::NativePreConstruct(){
	Super::NativePreConstruct();
	
	UpdateButtonText(ButtonText);
}

