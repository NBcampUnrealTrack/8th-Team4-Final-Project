#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterInGameMenuScreen.generated.h"

class UCommonButtonBase;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FOnInGameMenuAction);

/**
 * 인게임 시스템 메뉴의 표시와 버튼 입력만 담당한다.
 * 이동, 종료, 항복 규칙은 이 위젯 밖에서 처리한다.
 */
UCLASS(Abstract)
class FPS_API UShooterInGameMenuScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	UShooterInGameMenuScreen();

	void SetSurrenderEnabled(bool bEnabled);
	void SetStatusText(const FText& StatusText);

	FOnInGameMenuAction OnResumeRequested;
	FOnInGameMenuAction OnReturnToLobbyRequested;
	FOnInGameMenuAction OnQuitGameRequested;
	FOnInGameMenuAction OnSurrenderRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual bool NativeOnHandleBackAction() override;

private:
	void HandleResumeClicked();
	void HandleReturnToLobbyClicked();
	void HandleQuitGameClicked();
	void HandleSurrenderClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_Resume;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_ReturnToLobby;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_QuitGame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_Surrender;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;
};
