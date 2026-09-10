#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "GameMode/Types/DominationTypes.h"
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
	TObjectPtr<UTexture2D> WeaponArt;
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
	// PreviewClass는 임시 선택, bLocked는 확정 여부다
	void SetSelectionState(EDominationCombatClass PreviewClass, bool bClassLocked);

	FOnDominationClassCardClicked OnClassCardClicked;

protected:
	virtual void NativeOnClicked() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Class Selection")
	void UpdateSelectionPresentation(bool bIsSelected, bool bSelectionLocked);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_WeaponArt;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_ClassName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Description;

	EDominationCombatClass CombatClass = EDominationCombatClass::Unselected;
};
