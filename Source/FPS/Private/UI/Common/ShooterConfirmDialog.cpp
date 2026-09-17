// ShooterConfirmDialog.cpp
#include "UI/Common/ShooterConfirmDialog.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"

void UShooterConfirmDialog::NativeOnInitialized(){ 
	Super::NativeOnInitialized(); 

	Btn_Confirm->OnClicked().AddUObject(this, &ThisClass::HandleConfirmClicked);
	Btn_Cancel->OnClicked().AddUObject(this, &ThisClass::HandleCancelClicked);
}

UWidget* UShooterConfirmDialog::NativeGetDesiredFocusTarget() const{
	return Btn_Cancel;
}

void UShooterConfirmDialog::SetupMessage(const FText& InMessage){
	if (Text_Message)
	{
		Text_Message->SetText(InMessage);
	}
}

void UShooterConfirmDialog::HandleConfirmClicked(){
	OnConfirmed.Broadcast();
	DeactivateWidget();
}

void UShooterConfirmDialog::HandleCancelClicked(){
	OnCanceled.Broadcast();
	DeactivateWidget();
}
