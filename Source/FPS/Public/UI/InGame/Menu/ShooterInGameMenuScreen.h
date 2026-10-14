#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterInGameMenuScreen.generated.h"

class UTextBlock;
class UWidget;
class UWidgetAnimation;

DECLARE_MULTICAST_DELEGATE(FOnInGameMenuAction);

/**
 * 인게임 시스템 메뉴의 표시와 버튼 입력만 담당한다.
 * 이동, 종료, 항복 규칙은 이 위젯 밖에서 처리한다.
 *
 * BindWidget 타입을 UWidget으로 두어 기존 CommonUI 버튼과
 * 자동 생성 WBP의 일반 UMG Button을 모두 허용한다.
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
	FOnInGameMenuAction OnOptionsRequested;
	FOnInGameMenuAction OnReturnToLobbyRequested;
	FOnInGameMenuAction OnQuitGameRequested;
	FOnInGameMenuAction OnSurrenderRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;

private:
	void RequestResume();

	UFUNCTION()
	void HandleExitFinished();

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleOptionsClicked();

	UFUNCTION()
	void HandleReturnToLobbyClicked();

	UFUNCTION()
	void HandleQuitGameClicked();

	UFUNCTION()
	void HandleSurrenderClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Btn_Resume;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Btn_Options;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Btn_ReturnToLobby;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Btn_QuitGame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Btn_Surrender;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Enter;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Exit;

	FWidgetAnimationDynamicEvent ExitFinishedDelegate;

	bool bClosing = false;
};
