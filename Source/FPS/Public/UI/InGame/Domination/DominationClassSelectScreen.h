#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "UI/InGame/Domination/DominationClassSelectCard.h"
#include "DominationClassSelectScreen.generated.h"

class ADominationPlayerState;
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

private:
	UFUNCTION()
	void HandleCombatClassChanged(EDominationCombatClass SelectedClass);

	void HandleClassCardClicked(EDominationCombatClass RequestedClass);
	void RebuildClassCards();
	void ClearClassCards();
	void BindToPlayerState();
	void UnbindFromPlayerState();
	void RefreshSelectionState(EDominationCombatClass SelectedClass);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> Panel_ClassCards;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	TSubclassOf<UDominationClassSelectCard> ClassCardWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	FMargin ClassCardPadding = FMargin(8.0f);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDominationClassSelectCard>> ClassCards;

	TWeakObjectPtr<ADominationPlayerState> BoundPlayerState;
};
