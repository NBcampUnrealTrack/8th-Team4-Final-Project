// ShooterStartupScreen.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterStartupScreen.generated.h"

class UCommonActivatableWidget;
class UShooterButtonBase;
class UTextBlock;
class UWidgetAnimation;

/**
 * 첫 실행 인트로와 온라인 상태 확인
 */
UCLASS()
class FPS_API UShooterStartupScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	void RefreshOnlineState();

	UFUNCTION()
	void HandleIntroFinished();
	UFUNCTION()
	void HandleOpenFinished();
	void HandleLoginClicked();
	void HandleRetryClicked();
	void HandleQuitClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Login;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Retry;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Quit;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Anim_Intro;
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Anim_Open;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> HomeScreenClass;

	FWidgetAnimationDynamicEvent IntroFinishedDelegate;
	FWidgetAnimationDynamicEvent OpenFinishedDelegate;

	bool bOpening = false;
};
