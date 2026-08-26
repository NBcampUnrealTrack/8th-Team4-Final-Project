// ShooterUIInitComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "ShooterUIInitComponent.generated.h"

class ABaseRoundGameState;
class ABaseRoundPlayerController;
class UAugmentInventoryComponent;
class UCommonActivatableWidget;
class UShooterAugmentSelectScreen;
class UShooterInGameMenuScreen;
class UShooterShopScreen;
class UShooterSurrenderVoteScreen;
class UShooterUIManagerSubsystem;
enum class ESurrenderVoteStartResult : uint8;
struct FAugmentInventoryEntry;

/**
 * 로컬 PlayerController의 인게임 화면 수명주기를 조정한다.
 * 화면은 표시와 입력만 맡고, 게임 규칙은 Controller와 GameMode에 전달한다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPS_API UShooterUIInitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UI|System Menu")
	void ToggleInGameMenu();

	UFUNCTION(BlueprintCallable, Category = "UI|System Menu")
	void ShowInGameMenu();

	UFUNCTION(BlueprintCallable, Category = "UI|System Menu")
	void HideInGameMenu();

	UFUNCTION(BlueprintPure, Category = "UI|System Menu")
	bool IsInGameMenuOpen() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void ToggleShop();

	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void ShowShop();

	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void HideShop();

	UFUNCTION(BlueprintPure, Category = "UI|Shop")
	bool IsShopOpen() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowAugmentSelect(const TArray<FAugmentInventoryEntry>& InOffers);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideAugmentSelect();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void TryStartInGameUI();

	void ShowSurrenderVote();
	void HideSurrenderVote();

	void HandleResumeRequested();
	void HandleReturnToLobbyRequested();
	void HandleQuitGameRequested();
	void HandleSurrenderRequested();
	void HandleSurrenderVoteSubmitted(bool bApprove);
	void HandleSurrenderMenuRequested();
	void HandleSurrenderVoteStateChanged();
	void HandleSurrenderVoteStartResult(ESurrenderVoteStartResult Result);
	
  UFUNCTION()
  void HandleAugmentSelected(int32 OfferIndex);

  UFUNCTION()
  void HandleAugmentOffersChanged();

  UFUNCTION()
  void HandleRoundStateChanged();

  UFUNCTION()
  void HandleAugmentRerollRequested(int32 SlotIndex);

  UFUNCTION()
  void HandleShopCloseRequested();

	void ScheduleModalRefresh();
	void RefreshModalPresentation();
	void RefreshAugmentSelection();
	bool CanRequestSurrender() const;
	bool CanShowShop() const;
	void ScheduleInGameUIRetry();
	void UnbindRuntimeDelegates();
	void RemoveScreenFromLayer(FGameplayTag LayerTag, UCommonActivatableWidget* Screen);

	ABaseRoundPlayerController* GetRoundPlayerController() const;
	UShooterUIManagerSubsystem* GetUIManager() const;

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

  TWeakObjectPtr<UAugmentInventoryComponent> BoundInventory;
  TWeakObjectPtr<ABaseRoundGameState> BoundRoundGameState;
  TWeakObjectPtr<ABaseRoundPlayerController> BoundRoundPlayerController;

  FTimerHandle PlayerStateWaitTimerHandle;
  FTimerHandle ModalRefreshTimerHandle;

  bool bInGameUIReady = false;
  bool bInGameMenuRequested = false;
  bool bLastObservedVoteActive = false;
  bool bModalRefreshPending = false;
  bool bLoggedInitialWidgetPushFailure = false;

  TArray<int32> BuildRerollsRemaining(int32 OfferCount) const;
};
