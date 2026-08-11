// ShooterConfirmDialog.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterConfirmDialog.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConfirmDialogClosed);

class UCommonButtonBase;
class UCommonTextBlock;

UCLASS()
class FPS_API UShooterConfirmDialog : public UShooterActivatableWidget{
	GENERATED_BODY()
	
	
public:
	// PopUp창에 나올 안내 메세지
	// 실행 전에 메세지를 넣는다.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetupMessage(const FText& InMessage);
	
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnConfirmDialogClosed OnConfirmed;
	
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnConfirmDialogClosed OnCanceled;
	
protected:
	virtual void NativeOnInitialized() override;
	
	UFUNCTION()
	void HandleConfirmClicked();
	UFUNCTION()
	void HandleCancelClicked();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Message;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_Confirm;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_Cancel;


	
};
