#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "GameMode/DominationTypes.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Class Selection")
	TSoftObjectPtr<UTexture2D> Icon;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnDominationClassCardClicked,
	EDominationCombatClass);

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
	void SetSelectionState(EDominationCombatClass SelectedClass);

	FOnDominationClassCardClicked OnClassCardClicked;

protected:
	virtual void NativeOnClicked() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Class Selection")
	void UpdateSelectionPresentation(bool bIsSelected, bool bSelectionLocked);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_ClassName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Description;

	EDominationCombatClass CombatClass = EDominationCombatClass::Unselected;
};
