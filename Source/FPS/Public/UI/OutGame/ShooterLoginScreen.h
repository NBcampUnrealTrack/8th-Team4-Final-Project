// ShooterLoginScreen.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterLoginScreen.generated.h"

class UCommonButtonBase;

UCLASS()
class FPS_API UShooterLoginScreen : public UShooterActivatableWidget{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	
	UFUNCTION()
	void HandleEnterLobbyClicked();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_EnterLobby;
	
	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> LobbyWidgetClass;
};
