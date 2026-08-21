#pragma once

#include "CoreMinimal.h"
#include "Shop/ShopTypes.h"
#include "UI/Foundation/ShooterActivatableWidget.h"

#include "ShooterShopScreen.generated.h"

class UCreditWalletComponent;
class UPanelWidget;
class UShopComponent;
class UShooterShopItemRow;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShopCloseRequestedSignature);

/**
 * 준비 단계에서 표시되는 상점 화면이다.
 * Catalog를 읽어 상품 Row를 구성하고, 화면 표시 및 구매 요청 전달만 담당한다.
 * 가격 판정과 실제 거래는 ShopComponent / ShopPurchaseComponent에 위임한다.
 */
UCLASS(Abstract)
class FPS_API UShooterShopScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	UShooterShopScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void RequestClose();

	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void RequestPurchase(FGameplayTag ItemId);

	UPROPERTY(BlueprintAssignable, Category = "UI|Shop")
	FOnShopCloseRequestedSignature OnCloseRequested;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;

private:
	UFUNCTION()
	void HandleCreditsChanged(int32 PreviousCredits, int32 NewCredits);

	void HandleItemPurchaseRequested(FGameplayTag ItemId);

	void RefreshCredits();
	void RebuildItemRows();
	void RefreshItemRows();

	bool ShouldDisplayItem(const FShopItemDefinition& Definition) const;

private:
	// 상품 Row가 자동으로 추가될 컨테이너다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_Items;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Credits;

	// WBP_ShopItemRow를 지정한다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Shop")
	TSubclassOf<UShooterShopItemRow> ItemRowClass;

	// 비어 있으면 전체 Catalog를 표시한다.
	// 기본값은 현재 스탯 상점 범위인 HP / Shield / Damage다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Shop")
	TArray<EShopItemType> VisibleItemTypes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterShopItemRow>> ItemRows;

	TWeakObjectPtr<UCreditWalletComponent> BoundCreditWallet;
	TWeakObjectPtr<UShopComponent> BoundShopComponent;
};