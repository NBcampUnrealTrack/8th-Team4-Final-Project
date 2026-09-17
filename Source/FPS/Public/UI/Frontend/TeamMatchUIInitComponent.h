#pragma once

#include "Augment/AugmentTypes.h"
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TeamMatchUIInitComponent.generated.h"

enum class EAugmentSelectEntry : uint8;
class AShooterPlayerController;
class UAugmentInventoryComponent;
class UCommonActivatableWidget;
class UShooterAugmentSelectScreen;
class UShooterChatInputBox;
class UShooterInGameMenuScreen;
class UShooterOptionsScreen;
class UShooterSurrenderVoteScreen;
class UShooterUIManagerSubsystem;
class UShooterScoreboard;

enum class ESurrenderVoteStartResult : uint8;

/**
 * 팀 기반 매치에서 공통으로 사용하는 메뉴, 항복 투표, 증강 선택 화면을 조정한다.
 * 각 게임모드는 화면 클래스와 현재 페이즈에서 허용할지 여부만 제공한다.
 */
UCLASS(Abstract, ClassGroup = (UI))
class FPS_API UTeamMatchUIInitComponent : public UActorComponent
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

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowAugmentSelect(const TArray<FAugmentInventoryEntry>& InOffers);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideAugmentSelect();
	
	// UI policy, available before PlayerState/UI initialization and on clients.
	UFUNCTION(BlueprintPure, Category = "UI|Scoreboard")
	virtual bool SupportsScoreboard() const { return true; }

	UFUNCTION(BlueprintCallable, Category = "UI|Scoreboard")
	void ShowScoreboard();
	
	UFUNCTION(BlueprintCallable, Category = "UI|Scoreboard")
	void HideScoreboard();

	// Enter로 채팅창을 연다. CommonUI GameMenu 레이어에 Push되어 활성화되며,
	// 그 시점에 UShooterChatInputBox::NativeGetDesiredFocusTarget으로 포커스가 자동으로 잡힌다.
	UFUNCTION(BlueprintCallable, Category = "UI|Chat")
	UShooterChatInputBox* OpenChat();

	UFUNCTION(BlueprintCallable, Category = "UI|Chat")
	void CloseChat();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeTeamMatchUI(
		AShooterPlayerController* PlayerController,
		UAugmentInventoryComponent* Inventory);
	void ShutdownTeamMatchUI();
	void NotifyModeUIStateChanged();

	bool IsTeamMatchUIReady() const;
	bool IsSurrenderVoteOpen() const;
	UAugmentInventoryComponent* GetBoundAugmentInventory() const;
	UShooterUIManagerSubsystem* GetUIManager() const;
	void RemoveScreenFromLayer(FGameplayTag LayerTag, UCommonActivatableWidget* Screen);
	
	void ResetAugmentRequestContext();
	bool IsAugmentSelectionRequestPending() const;
	
	virtual void OnAugmentScreenCreated(UShooterAugmentSelectScreen* Screen) {};
	virtual void OnAugmentOffersPresented(const TArray<FAugmentInventoryEntry>& InOffers) {};
	virtual EAugmentSelectEntry GetAugmentEntryAnimation() const;
	
	virtual bool IsAugmentPresentationAllowed() const
		PURE_VIRTUAL(UTeamMatchUIInitComponent::IsAugmentPresentationAllowed, return false;);
	virtual bool IsSurrenderRequestAllowed() const
		PURE_VIRTUAL(UTeamMatchUIInitComponent::IsSurrenderRequestAllowed, return false;);
	virtual void CloseModeSpecificScreensForModal() {}

	virtual TSubclassOf<UShooterInGameMenuScreen> GetInGameMenuWidgetClass() const
		PURE_VIRTUAL(UTeamMatchUIInitComponent::GetInGameMenuWidgetClass, return nullptr;);
	virtual TSubclassOf<UShooterSurrenderVoteScreen> GetSurrenderVoteWidgetClass() const
		PURE_VIRTUAL(UTeamMatchUIInitComponent::GetSurrenderVoteWidgetClass, return nullptr;);
	virtual TSubclassOf<UShooterAugmentSelectScreen> GetAugmentSelectWidgetClass() const
		PURE_VIRTUAL(UTeamMatchUIInitComponent::GetAugmentSelectWidgetClass, return nullptr;);

	virtual UShooterInGameMenuScreen* GetInGameMenuInstance() const
		PURE_VIRTUAL(UTeamMatchUIInitComponent::GetInGameMenuInstance, return nullptr;);
	virtual void SetInGameMenuInstance(UShooterInGameMenuScreen* Instance)
		PURE_VIRTUAL(UTeamMatchUIInitComponent::SetInGameMenuInstance, );
	virtual UShooterSurrenderVoteScreen* GetSurrenderVoteInstance() const
		PURE_VIRTUAL(UTeamMatchUIInitComponent::GetSurrenderVoteInstance, return nullptr;);
	virtual void SetSurrenderVoteInstance(UShooterSurrenderVoteScreen* Instance)
		PURE_VIRTUAL(UTeamMatchUIInitComponent::SetSurrenderVoteInstance, );
	virtual UShooterAugmentSelectScreen* GetAugmentSelectInstance() const
		PURE_VIRTUAL(UTeamMatchUIInitComponent::GetAugmentSelectInstance, return nullptr;);
	virtual void SetAugmentSelectInstance(UShooterAugmentSelectScreen* Instance)
		PURE_VIRTUAL(UTeamMatchUIInitComponent::SetAugmentSelectInstance, );

private:
	void PresentInGameMenu();
	void PresentOptionsScreen();
	void RemoveInGameMenu();
	void RemoveOptionsScreen();
	void PresentSurrenderVote();
	void RemoveSurrenderVote();
	void ScheduleModalRefresh();
	void RefreshModalPresentation();
	bool CanRequestSurrender() const;
	TArray<int32> BuildRerollsRemaining(int32 OfferCount) const;

	void HandleResumeRequested();
	void HandleOptionsRequested();
	void HandleOptionsCloseRequested();
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
	void HandleAugmentRerollRequested(int32 SlotIndex);
	
	UFUNCTION()
	void HandleSelectionOutroFinished();

	// 모든 팀 매치가 공통으로 사용하는 옵션 화면. 파생 UI Init BP에서 WBP_OptionsScreen을 지정한다.
	UPROPERTY(EditAnywhere, Category = "UI|System Menu")
	TSubclassOf<UShooterOptionsScreen> OptionsScreenClass;

	UPROPERTY(Transient)
	TObjectPtr<UShooterOptionsScreen> OptionsScreenInstance;

	UPROPERTY(EditAnywhere, Category = "UI|Scoreboard")
	TSubclassOf<UShooterScoreboard> ScoreboardClass;
	UPROPERTY(Transient)
	TObjectPtr<UShooterScoreboard> ScoreboardInstance;

	UPROPERTY(EditAnywhere, Category = "UI|Chat")
	TSubclassOf<UShooterChatInputBox> ChatInputBoxClass;
	UPROPERTY(Transient)
	TObjectPtr<UShooterChatInputBox> ChatInputBoxInstance;

	TWeakObjectPtr<AShooterPlayerController> BoundPlayerController;
	TWeakObjectPtr<UAugmentInventoryComponent> BoundInventory;
	FTimerHandle ModalRefreshTimerHandle;
	bool bTeamMatchUIReady = false;
	bool bInGameMenuRequested = false;
	bool bLastObservedVoteActive = false;
	bool bModalRefreshPending = false;
	
	// Offers가 비었다 - 카드가 적용 됐다는 뜻이 아님
	bool bSelectionRequestPending = false;
	
	int32 RequestedOfferIndex = INDEX_NONE;
	FPrimaryAssetId RequestDefinitionId;
};
