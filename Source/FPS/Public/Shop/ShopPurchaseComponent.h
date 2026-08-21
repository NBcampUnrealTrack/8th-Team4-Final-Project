#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Shop/ShopTypes.h"

#include "ShopPurchaseComponent.generated.h"

class UShopComponent;

UCLASS(ClassGroup = (Economy))
class FPS_API UShopPurchaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShopPurchaseComponent();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Shop")
	void ServerPurchaseItem(FGameplayTag ItemId);

	// 라운드 시작 시 Shield처럼 Pending 상태인 구매만 최종 확정한다.
	bool CommitPendingPurchases();

private:
	bool TryPurchaseItem(FGameplayTag ItemId);

	// HP / Damage처럼 구매 즉시 확정되는 상품 처리.
	bool TryPurchaseImmediateItem(
		UShopComponent& ShopComponent,
		const FShopItemDefinition& Definition);

	// Pending을 사용하는 상품의 현재 거래 핸들을 반환한다.
	FShopTransactionHandle* GetPendingTransaction(
		EShopItemType ItemType);

	// 상품 타입별 실제 지급을 연결한다.
	bool ApplyPurchasedItem(
		const FShopItemDefinition& Definition);

	// GameplayEffect 기반 상품을 ASC에 적용한다.
	bool ApplyGameplayEffectPurchase(
		const FShopItemDefinition& Definition);

	// Pending 상품 지급 실패 시 이전 거래 상태로 복구한다.
	void RollbackFailedApplication(
		UShopComponent& ShopComponent,
		const FShopTransactionHandle& PreviousTransaction,
		const FShopTransactionHandle& CurrentTransaction,
		FShopTransactionHandle& PendingTransaction);

private:
	// Shield는 준비 페이즈 동안 Small/Large 교체가 가능하므로 Pending으로 유지한다.
	FShopTransactionHandle PendingShieldTransaction;
};