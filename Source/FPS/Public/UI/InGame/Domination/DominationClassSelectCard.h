#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "GameMode/Types/DominationTypes.h"
#include "UI/InGame/Domination/DominationCardHoverController.h"
#include "DominationClassSelectCard.generated.h"

class UCommonTextBlock;
class UImage;
class UTexture2D;

USTRUCT(BlueprintType)
struct FPS_API FDominationClassCardPresentation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Class Selection")
	EDominationCombatClass CombatClass = EDominationCombatClass::Unselected;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Class Selection")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Class Selection")
	FText Description;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Portrait;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnDominationClassCardClicked,
	EDominationCombatClass);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnDominationClassCardHovered,
	EDominationCombatClass,
	bool /*bHovered*/);

/**
 * 전투 클래스 하나를 표시하는 선택 카드다.
 * 표시 데이터와 클릭한 클래스만 화면에 전달하며 선택 규칙은 소유하지 않는다.
 */
UCLASS(Abstract)
class FPS_API UDominationClassSelectCard : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void SetClassData(const FDominationClassCardPresentation& Presentation);
	// PreviewClass는 임시 선택, bLocked는 확정 여부
	void SetSelectionState(EDominationCombatClass PreviewClass, bool bClassLocked);
	void ApplyHoverState(const FDominationCardHoverState& State);

	// 화면이 hover 인덱스를 찾을 때 사용
	EDominationCombatClass GetCombatClass() const;

	FOnDominationClassCardClicked OnClassCardClicked;
	FOnDominationClassCardHovered OnClassCardHovered;

protected:
	virtual void NativeOnClicked() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Class Selection")
	void BP_UpdateSelectionPresentation(bool bIsSelected, bool bSelectionLocked);


private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_PickArt;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_ClassName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Description;

	EDominationCombatClass CombatClass = EDominationCombatClass::Unselected;
};
