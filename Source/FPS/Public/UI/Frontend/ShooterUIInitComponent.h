// ShooterUIInitComponent.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "UI/Frontend/TeamMatchUIInitComponent.h"
#include "ShooterUIInitComponent.generated.h"

class ABaseRoundGameState;
class ABaseRoundPlayerController;
class UAugmentInventoryComponent;
class UCommonActivatableWidget;
class UShooterAugmentSelectScreen;
class UShooterInGameMenuScreen;
class UShooterShopScreen;
class UShooterSurrenderVoteScreen;

/**
 * 로컬 PlayerController의 인게임 화면 수명주기를 조정한다.
 * 화면은 표시와 입력만 맡고, 게임 규칙은 Controller와 GameMode에 전달한다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPS_API UShooterUIInitComponent : public UTeamMatchUIInitComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void ToggleShop();

	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void ShowShop();

	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void HideShop();

	UFUNCTION(BlueprintPure, Category = "UI|Shop")
	bool IsShopOpen() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsAugmentPresentationAllowed() const override;
	virtual bool IsSurrenderRequestAllowed() const override;
	virtual void CloseModeSpecificScreensForModal() override;

	virtual TSubclassOf<UShooterInGameMenuScreen> GetInGameMenuWidgetClass() const override;
	virtual TSubclassOf<UShooterSurrenderVoteScreen> GetSurrenderVoteWidgetClass() const override;
	virtual TSubclassOf<UShooterAugmentSelectScreen> GetAugmentSelectWidgetClass() const override;
	virtual UShooterInGameMenuScreen* GetInGameMenuInstance() const override;
	virtual void SetInGameMenuInstance(UShooterInGameMenuScreen* Instance) override;
	virtual UShooterSurrenderVoteScreen* GetSurrenderVoteInstance() const override;
	virtual void SetSurrenderVoteInstance(UShooterSurrenderVoteScreen* Instance) override;
	virtual UShooterAugmentSelectScreen* GetAugmentSelectInstance() const override;
	virtual void SetAugmentSelectInstance(UShooterAugmentSelectScreen* Instance) override;

private:
	void TryStartInGameUI();

  UFUNCTION()
  void HandleRoundStateChanged();

  UFUNCTION()
  void HandleShopCloseRequested();

	bool CanShowShop() const;
	void ScheduleInGameUIRetry();
	void UnbindRuntimeDelegates();

	ABaseRoundPlayerController* GetRoundPlayerController() const;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UShooterPrimaryGameLayout> RootLayoutClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCommonActivatableWidget> InitialWidgetClass;

	UPROPERTY(EditAnywhere, Category = "UI", meta = (Categories = "UI.Layer"))
	FGameplayTag InitialWidgetLayer;

	UPROPERTY(EditAnywhere, Category = "UI|System Menu")
	TSubclassOf<UShooterInGameMenuScreen> InGameMenuClass;

	UPROPERTY(EditAnywhere, Category = "UI|Surrender")
	TSubclassOf<UShooterSurrenderVoteScreen> SurrenderVoteClass;

	UPROPERTY(EditAnywhere, Category = "UI|Shop")
	TSubclassOf<UShooterShopScreen> ShopScreenClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UShooterAugmentSelectScreen> AugmentSelectClass;

	UPROPERTY(Transient)
	TObjectPtr<UShooterInGameMenuScreen> InGameMenuInstance;

	UPROPERTY(Transient)
	TObjectPtr<UShooterSurrenderVoteScreen> SurrenderVoteInstance;

	UPROPERTY(Transient)
	TObjectPtr<UShooterShopScreen> ShopScreenInstance;

	UPROPERTY(Transient)
	TObjectPtr<UShooterAugmentSelectScreen> AugmentSelectInstance;

  TWeakObjectPtr<ABaseRoundGameState> BoundRoundGameState;

  FTimerHandle PlayerStateWaitTimerHandle;

  bool bLoggedInitialWidgetPushFailure = false;
};
