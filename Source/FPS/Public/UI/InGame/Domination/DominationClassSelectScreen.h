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
UCLASS(Abstract)
class FPS_API UDominationClassSelectScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	UDominationClassSelectScreen();

	UTexture2D* GetCachedClassIcon(EDominationCombatClass CombatClass) const;
	UTexture2D* GetCachedClassPortrait(EDominationCombatClass CombatClass) const;
	UTexture2D* GetCachedClassWeaponArt(EDominationCombatClass CombatClass) const;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// LOCK IN → PICKED 라벨 전환은 WBP가 담당
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Class Selection")
	void UpdateLockInPresentation(bool bLocked, bool bHasPreview, bool bRequestPending);

private:
	UFUNCTION()
	void HandleCombatClassChanged(EDominationCombatClass SelectedClass);

	UFUNCTION()
	void HandlePreviewCombatClassChanged(EDominationCombatClass PreviewClass);

	void HandleClassCardClicked(EDominationCombatClass RequestedClass);
	void HandleLockInClicked();
	void BuildIconCache();
	void RebuildClassCards();
	void ClearClassCards();
	void BindToPlayerState();
	void UnbindFromPlayerState();
	void RefreshSelectionState();
	void RefreshLockInButton();
	void RefreshTimerText();
	void RefreshClassInfoPanel();

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
	TSubclassOf<UDominationClassSelectCard> ClassCardWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	FMargin ClassCardPadding = FMargin(8.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	TSubclassOf<UDominationPlayerSlot> PlayerSlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	TSubclassOf<ADominationClassPreviewStage> PreviewStageClass;

	// 게임 카메라에 안 보이기만 하면 됨
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	FVector PreviewStageSpawnLocation = FVector(500000.0f, 500000.0f, 0.0f);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDominationClassSelectCard>> ClassCards;
	UPROPERTY(Transient)
	TMap<EDominationCombatClass, TObjectPtr<UTexture2D>> ClassIconCache;
	UPROPERTY(Transient)
	TMap<EDominationCombatClass, TObjectPtr<UTexture2D>> ClassPortraitCache;
	UPROPERTY(Transient)
	TMap<EDominationCombatClass, TObjectPtr<UTexture2D>> ClassWeaponArtCache;

	UPROPERTY(Transient)
	TArray<FDominationPlayerSlotEntry> SlotEntries;
	UPROPERTY(Transient)
	TObjectPtr<ADominationClassPreviewStage> PreviewStage;

	TWeakObjectPtr<ADominationPlayerState> BoundPlayerState;

	FTimerHandle TimerTextHandle;
	FTimerHandle ReconcileTimerHandle;

	// 확정 요청 복제가 돌아오기 전까지의 왕복 구간을 막는 로컬 플래그
	bool bLockInRequestPending = false;
};
