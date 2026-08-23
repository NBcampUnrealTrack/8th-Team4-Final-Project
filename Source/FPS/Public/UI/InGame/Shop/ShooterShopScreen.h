#pragma once

#include "CoreMinimal.h"
#include "Shop/ShopTypes.h"
#include "UI/Foundation/ShooterActivatableWidget.h"

#include "ShooterShopScreen.generated.h"

class UCreditWalletComponent;
class UPanelWidget;
class UShopComponent;
class UShopPurchaseComponent;
class UShooterShopItemRow;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShopCloseRequestedSignature);

/**
 * 준비 단계에서 표시되는 상점 화면이다.
 * Catalog 상품을 타입별 패널에 자동 배치하고, 표시 및 구매 요청 전달만 담당한다.
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

	UFUNCTION()
	void HandlePendingStateChanged();

	void HandleItemPurchaseRequested(FGameplayTag ItemId);

	void RefreshCredits();
	void RebuildItemRows();
	void RefreshItemRows();
	void ClearItemPanels();

	UPanelWidget* GetTargetPanel(const FShopItemDefinition& Definition) const;

private:
	// 주무기 상품이 자동으로 추가될 컨테이너다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_PrimaryWeapons;

	// 보호막 상품이 자동으로 추가될 컨테이너다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_Shields;

	// 무기/체력/공격력 업그레이드가 자동으로 추가될 컨테이너다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_Upgrades;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Credits;

	// 상품별 WidgetClassOverride가 없을 때 사용할 공통 카드 Widget이다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Shop")
	TSubclassOf<UShooterShopItemRow> ItemRowClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterShopItemRow>> ItemRows;

	TWeakObjectPtr<UCreditWalletComponent> BoundCreditWallet;
	TWeakObjectPtr<UShopComponent> BoundShopComponent;
	TWeakObjectPtr<UShopPurchaseComponent> BoundPurchaseComponent;
};
