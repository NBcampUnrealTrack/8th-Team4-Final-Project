// ShooterLobbyScreen.h
#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ShooterLobbyScreen.generated.h"

class UCommonButtonBase;

UCLASS()
class FPS_API UShooterLobbyScreen : public UCommonActivatableWidget{
	GENERATED_BODY()
	
protected:
	virtual void NativeOnInitialized() override;
	
	void HandlePlayClicked();
		
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_Play;

	// Test를 위한 임시로 Play를 눌렀을때 열 맵
	// 매치메이킹이 확정되면 삭제
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftObjectPtr<UWorld> TempDestinationMap;
};
