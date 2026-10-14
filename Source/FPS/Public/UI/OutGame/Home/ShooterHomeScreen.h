// ShooterHomeScreen.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterHomeScreen.generated.h"

class UCommonActivatableWidget;
class UShooterButtonBase;
class UShooterConfirmDialog;
class UShooterOptionsScreen;
class UWorld;

/**
 * 프론트엔드 첫 화면
 */
UCLASS(meta = (DisableNativeTick))
class FPS_API UShooterHomeScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	void HandlePlayClicked();
	void HandleTrainingClicked();
	void HandleCustomizeClicked();
	void HandleSettingsClicked();
	void HandleQuitClicked();
	void HandleOptionsCloseRequested();

	UFUNCTION()
	void HandleQuitConfirmed();
	UFUNCTION()
	void HandleQuitCanceled();
	void ClearQuitConfirmBindings();

	void PushMenuScreen(const TSoftClassPtr<UCommonActivatableWidget>& ScreenClass);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Play;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Training;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Settings;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Quit;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UShooterButtonBase> Btn_Customize;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> SessionBrowserClass;
	UPROPERTY(EditDefaultsOnly, Category = "Travel")
	TSoftObjectPtr<UWorld> TrainingDestinationMap;
	UPROPERTY(EditDefaultsOnly, Category = "Travel")
	TSoftObjectPtr<UWorld> CustomizationDestinationMap;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UShooterOptionsScreen> OptionsScreenClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UShooterConfirmDialog> ConfirmDialogClass;

	TWeakObjectPtr<UShooterOptionsScreen> OptionsScreenInstance;
	TWeakObjectPtr<UShooterConfirmDialog> QuitConfirmDialog;
};
