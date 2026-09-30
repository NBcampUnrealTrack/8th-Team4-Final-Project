#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "UI/InGame/Domination/DominationClassSelectCard.h"
#include "DominationClassSelectScreen.generated.h"

class ADominationClassPreviewStage;
class ADominationPlayerState;
class UCommonButtonBase;
class UCommonTextBlock;
class UDominationClassInfoPanel;
class UDominationPlayerSlot;
class UHorizontalBox;
class UImage;
class UPanelWidget;
class UTexture2D;
class USoundBase;
class UAudioComponent;

USTRUCT()
struct FDominationPlayerSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UDominationPlayerSlot> Slot = nullptr;
	TWeakObjectPtr<ADominationPlayerState> PlayerState;
	int32 PlayerId = INDEX_NONE;
};

/**
 * 클래스 선택 화면이다.
 * 설정된 클래스 카드를 생성하고 기존 Domination 선택 요청으로 전달한다.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class FPS_API UDominationClassSelectScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	UDominationClassSelectScreen();
	void NotifySelectionFailed(const FText& Reason);
	void SetAllowActiveClassReselection(bool bAllow);

	UTexture2D* GetCachedClassIcon(EDominationCombatClass CombatClass) const;
	UTexture2D* GetCachedClassPortrait(EDominationCombatClass CombatClass) const;
	UTexture2D* GetCachedClassWeaponArt(EDominationCombatClass CombatClass) const;
	USoundBase* GetCachedSelectionVoice(EDominationCombatClass CombatClass) const;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// LOCK IN → PICKED 라벨 전환은 WBP가 담당
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Class Selection")
	void BP_UpdateLockInPresentation(bool bLocked, bool bHasPreview, bool bRequestPending);

private:
	UFUNCTION()
	void HandleCombatClassChanged(EDominationCombatClass SelectedClass);
	UFUNCTION()
	void HandlePreviewCombatClassChanged(EDominationCombatClass PreviewClass);

	void HandleClassCardClicked(EDominationCombatClass RequestedClass);
	void HandleClassCardHovered(EDominationCombatClass HoveredClass, bool bHovered);
	void StartHoverUpdates();
	void UpdateHoverAnimation();
	void HandleLockInClicked();
	void RequestClassPresentationAssets();
	void BuildIconCache();
	void RebuildClassCards();
	void ClearClassCards();
	void BindToPlayerState();
	void UnbindFromPlayerState();
	void RefreshSelectionState();
	void RefreshLockInButton();
	void RefreshTimerText();
	void RefreshClassInfoPanel();
	void RefreshSelectionModePresentation();
	void HandleAudioSettingsChanged();
	void ApplyBGMSettings();
	float ResolveBGMComponentVolume();
	bool IsActiveClassReselectionMode() const;

	// 팀원 슬롯 수명. PlayerArray를 주기적으로 훑어 자리를 맞춘다.
	void ReconcileSlots();
	void RefreshAllSlots();
	void RebuildSlotPanel();
	void ClearSlotEntries();
	void BindSlotPlayerState(
		FDominationPlayerSlotEntry& Entry,
		ADominationPlayerState* PlayerState,
		bool bIsLocalPlayer);
	void UnbindSlotPlayerState(FDominationPlayerSlotEntry& Entry);

	// 캐릭터 프리뷰
	void SpawnPreviewStage();
	void DestroyPreviewStage();
	void RefreshPreviewStage();

	EDominationCombatClass GetDisplayCombatClass() const;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> Panel_ClassCards;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDominationClassInfoPanel> Panel_ClassInfo;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_PlayerSlots;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_CharacterPreview;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Button_LockIn;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Timer;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	FMargin ClassCardPadding = FMargin(8.0f);
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	FDominationCardHoverSettings CardHoverSettings;
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	TSubclassOf<UDominationClassSelectCard> ClassCardWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	TSubclassOf<UDominationPlayerSlot> PlayerSlotWidgetClass;
	// 끄면 메타휴먼 프리뷰를 스폰하지 않음
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	bool bEnableCharacterPreview = true;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	TSubclassOf<ADominationClassPreviewStage> PreviewStageClass;

	// 게임 카메라에 안 보이기만 하면 됨
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	FVector PreviewStageSpawnLocation = FVector(500000.0f, 500000.0f, 0.0f);
	
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Audio")
	TObjectPtr<USoundBase> ClassSelectBGM;
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Audio")
	TObjectPtr<USoundBase> LockInSound;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BGMComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDominationClassSelectCard>> ClassCards;
	UPROPERTY(Transient)
	TMap<EDominationCombatClass, TObjectPtr<UTexture2D>> ClassIconCache;
	UPROPERTY(Transient)
	TMap<EDominationCombatClass, TObjectPtr<UTexture2D>> ClassPortraitCache;
	UPROPERTY(Transient)
	TMap<EDominationCombatClass, TObjectPtr<USoundBase>> ClassVoiceCache;
	UPROPERTY(Transient)
	TMap<EDominationCombatClass, TObjectPtr<UTexture2D>> ClassWeaponArtCache;

	UPROPERTY(Transient)
	TArray<FDominationPlayerSlotEntry> SlotEntries;
	UPROPERTY(Transient)
	TObjectPtr<ADominationClassPreviewStage> PreviewStage;

	TSharedPtr<struct FStreamableHandle> PresentationLoadHandle;
	uint32 PresentationLoadGeneration = 0;

	FDominationCardHoverController CardHoverController;

	TWeakObjectPtr<ADominationPlayerState> BoundPlayerState;

	FTimerHandle TimerTextHandle;
	FTimerHandle ReconcileTimerHandle;
	FTimerHandle HoverUpdateTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection", meta = (ClampMin = "0.008", ClampMax = "0.05"))
	float HoverUpdateInterval = 1.0f / 60.0f;

	// 확정 요청 복제가 돌아오기 전까지의 왕복 구간을 막는 로컬 플래그
	bool bLockInRequestPending = false;
	bool bAllowActiveClassReselection = false;
	EDominationCombatClass ActiveReselectionPreviewClass = EDominationCombatClass::Unselected;
	FText SelectionFailure;
};
