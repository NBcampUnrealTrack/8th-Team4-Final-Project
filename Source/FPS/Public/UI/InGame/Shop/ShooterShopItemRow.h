#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shop/ShopCatalogDataAsset.h"

#include "ShooterShopItemRow.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnShopItemPurchaseRequested,
	FGameplayTag);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnShopItemRefundRequested,
	FGameplayTag);

/**
 * 상점 상품 하나를 표시하는 공통 카드다.
 * Catalog의 이름/아이콘과 서버가 계산한 가격 상태를 표시하고,
 * 클릭 시 ItemId만 상위 ShopScreen에 전달한다.
 */
UCLASS(Abstract)
class FPS_API UShooterShopItemRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetItemData(
		const FShopItemDefinition& Definition,
		const FShopItemPresentation& Presentation);

	void RefreshPurchaseState(
		int32 Price,
		EShopPurchaseResult PurchaseResult,
		bool bIsPending,
		bool bIsEquippedPending);

	FGameplayTag GetItemId() const
	{
		return ItemId;
	}

	FOnShopItemPurchaseRequested OnPurchaseRequested;
	FOnShopItemRefundRequested OnRefundRequested;

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandlePurchaseClicked();

	UFUNCTION()
	void HandleRefundClicked();

private:
	FGameplayTag ItemId;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Price;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_State;

	// 카드 전체를 클릭 영역으로 사용한다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Purchase;

	// 확정 전 상품을 취소하는 별도 버튼이다. 기존 WBP와의 호환을 위해 선택 바인딩으로 둔다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Refund;
};
