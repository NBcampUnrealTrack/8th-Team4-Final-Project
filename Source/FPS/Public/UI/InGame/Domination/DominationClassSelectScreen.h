#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "UI/InGame/Domination/DominationClassSelectCard.h"
#include "DominationClassSelectScreen.generated.h"

class ADominationPlayerState;
class UCommonButtonBase;
class UCommonTextBlock;
class UHorizontalBox;

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
	void RebuildClassCards();
	void ClearClassCards();
	void BindToPlayerState();
	void UnbindFromPlayerState();
	void RefreshSelectionState();
	void RefreshLockInButton();
	void RefreshTimerText();

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> Panel_ClassCards;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Button_LockIn;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Timer;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	TSubclassOf<UDominationClassSelectCard> ClassCardWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	FMargin ClassCardPadding = FMargin(8.0f);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDominationClassSelectCard>> ClassCards;

	TWeakObjectPtr<ADominationPlayerState> BoundPlayerState;

	FTimerHandle TimerTextHandle;

	// 확정 요청 복제가 돌아오기 전까지의 왕복 구간을 막는 로컬 플래그
	bool bLockInRequestPending = false;
};
