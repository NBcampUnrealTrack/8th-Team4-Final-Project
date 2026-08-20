#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shop/ShopTypes.h"

#include "ShopComponent.generated.h"

class UCreditWalletComponent;
class URogueShopSettings;

/**
 * 플레이어별 상점 거래를 관리한다.
 * 상품, 구매 페이즈, 잔액을 서버에서 검증하고 CreditWallet을 통해 결제를 처리한다.
 * 실제 상품 지급, GAS 적용, 해금 및 장착 상태 변경은 외부 시스템의 책임이다.
 */
UCLASS(ClassGroup = (Economy))
class FPS_API UShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShopComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Shop")
	bool FindItemDefinition(FGameplayTag ItemId, FShopItemDefinition& OutDefinition) const;

	// UI에서 구매 가능 상태를 미리 조회할 수 있지만 실제 거래 시 서버가 동일한 규칙을 다시 검증한다.
	UFUNCTION(BlueprintPure, Category = "Shop")
	EShopPurchaseResult GetPurchaseEligibility(FGameplayTag ItemId, int32& OutPrice) const;

	/**
	 * 서버에서 구매 조건을 검증하고 크레딧을 차감해 미확정 거래를 생성한다.
	 * 성공한 거래는 반드시 CommitTransaction 또는 RefundTransaction으로 종료해야 한다.
	 */
	FShopTransactionResult TryBeginTransaction(FGameplayTag ItemId);

	// 외부 시스템의 상품 적용이 성공한 거래를 최종 확정한다.
	EShopPurchaseResult CommitTransaction(const FShopTransactionHandle& Transaction);

	// 상품 적용이 취소되거나 실패한 거래의 실제 결제 금액을 환불한다.
	EShopPurchaseResult RefundTransaction(const FShopTransactionHandle& Transaction);

	// 모든 미확정 거래를 환불하며 실패한 거래는 원장에 남겨 재시도할 수 있게 한다.
	bool RefundAllActiveTransactions();

	/**
	 * Wallet을 변경하지 않고 모든 미확정 거래를 폐기한다.
	 * 새 경기에서 전체 경제를 초기화하기 직전에만 사용한다.
	 */
	bool DiscardAllActiveTransactionsForNewMatch();

	// 새 경기 초기화를 위해 거래 원장을 안전하게 폐기할 수 있는 상태인지 확인한다.
	bool CanDiscardAllActiveTransactionsForNewMatch(FString* OutFailureReason = nullptr) const;

	bool HasActiveTransaction(const FGuid& TransactionId) const;
	int32 GetActiveTransactionCount() const { return ActiveTransactions.Num(); }

#if WITH_DEV_AUTOMATION_TESTS
	// 실제 GameState 페이즈를 구성하지 않는 자동화 테스트에서만 구매 가능 상태를 대체한다.
	void SetPurchaseAvailabilityOverrideForTesting(bool bEnabled, bool bShopOpen);
#endif

private:
	// 구매 검증에서 조회한 상품, 가격, Wallet을 하나의 결과로 묶어 동일한 값을 후속 처리에 사용한다.
	struct FShopPurchaseResolution
	{
		EShopPurchaseResult Result = EShopPurchaseResult::InvalidItem;
		const FShopItemDefinition* Definition = nullptr;
		UCreditWalletComponent* Wallet = nullptr;
		int32 Price = 0;
	};

	FShopPurchaseResolution ResolvePurchaseRequest(FGameplayTag ItemId) const;
	const URogueShopSettings* GetShopSettings() const;
	UCreditWalletComponent* GetCreditWallet() const;
	int32 ResolvePrice(const FShopItemDefinition& Definition) const;
	bool IsPurchaseWindowOpen(const URogueShopSettings& Settings) const;
	bool HasServerAuthority() const;

	EShopPurchaseResult ValidateActiveTransaction(
		const FShopTransactionHandle& Transaction,
		const FShopTransactionHandle*& OutActiveTransaction) const;

private:
	// 결제됐지만 아직 확정 또는 환불되지 않은 서버 전용 거래 원장이다.
	TMap<FGuid, FShopTransactionHandle> ActiveTransactions;

	// Wallet 변경 델리게이트 등으로 같은 거래 처리에 재진입하는 것을 차단한다.
	bool bIsProcessingTransaction = false;

#if WITH_DEV_AUTOMATION_TESTS
	bool bHasPurchaseAvailabilityOverrideForTesting = false;
	bool bPurchaseAvailabilityOverrideForTesting = false;
#endif
};