#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shop/ShopTypes.h"

#include "ShooterShopItemRow.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnShopItemPurchaseRequested,
	FGameplayTag);

/**
 * 상점 상품 하나를 표시하는 공통 Row.
 * 상품 정의와 서버 조회 결과를 표현하고,
 * 클릭 시 ItemId만 상위 ShopScreen에 전달한다.
 */
UCLASS(Abstract)
class FPS_API UShooterShopItemRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetItemDefinition(
		const FShopItemDefinition& Definition);

	void RefreshPurchaseState(
		int32 Price,
		EShopPurchaseResult PurchaseResult);

	FGameplayTag GetItemId() const
	{
		return ItemId;
	}

	FOnShopItemPurchaseRequested OnPurchaseRequested;

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandlePurchaseClicked();

private:
	FGameplayTag ItemId;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Price;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_State;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Purchase;
};
