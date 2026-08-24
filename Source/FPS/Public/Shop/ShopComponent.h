#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shop/ShopTypes.h"

#include "ShopComponent.generated.h"

class UCreditWalletComponent;
class URogueShopSettings;
class UShopCatalogDataAsset;

DECLARE_MULTICAST_DELEGATE(FOnShopPurchaseStateChanged);

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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Shop")
	bool FindItemDefinition(
		FGameplayTag ItemId,
		FShopItemDefinition& OutDefinition) const;

	// CreditOffset은 확정 전에 이미 지불한 금액이다. 교체 구매는 그만큼을 여유 크레딧으로 인정하므로,
	// UI 사전 조회가 실제 구매와 같은 결과를 내려면 같은 값을 넘겨야 한다.
	// Pending을 아는 것은 ShopPurchaseComponent이므로 이 값은 호출자가 정한다.
	UFUNCTION(BlueprintPure, Category = "Shop")
	EShopPurchaseResult GetPurchaseEligibility(
		FGameplayTag ItemId,
		int32& OutPrice,
		int32 CreditOffset = 0) const;

	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetConfirmedPurchaseCount(FGameplayTag ItemId) const;

	// 거래를 만들지 않는 무료 장착도 서버와 동일한 구매 페이즈 판정을 사용한다.
	UFUNCTION(BlueprintPure, Category = "Shop")
	bool IsPurchaseWindowOpen() const;

	// 확정 구매 상태가 클라이언트에 갱신됐을 때 UI 등 조회 계층에 알린다.
	FOnShopPurchaseStateChanged OnPurchaseStateChanged;

	/**
	 * 서버에서 구매 조건을 검증하고 크레딧을 차감해 미확정 거래를 생성한다.
	 * 성공한 거래는 반드시 CommitTransaction 또는 RefundTransaction으로 종료해야 한다.
	 */
	FShopTransactionResult TryBeginTransaction(FGameplayTag ItemId);

	// 진행 중인 같은 종류의 거래를 다른 상품으로 교체하고 가격 차액만 정산한다.
	FShopTransactionResult TryReplaceTransaction(
		const FShopTransactionHandle& CurrentTransaction,
		FGameplayTag NewItemId);

	// 거래 원장의 현재 값과 요청 핸들이 정확히 일치하는지 변경 없이 확인한다.
	// 라운드 경계에서는 모든 플레이어의 거래를 먼저 사전검증한 뒤 확정한다.
	EShopPurchaseResult CanCommitTransaction(
		const FShopTransactionHandle& Transaction) const;

	// 외부 시스템의 상품 적용이 성공한 거래를 최종 확정한다.
	EShopPurchaseResult CommitTransaction(
		const FShopTransactionHandle& Transaction);

	// 상품 적용이 취소되거나 실패한 거래의 실제 결제 금액을 환불한다.
	EShopPurchaseResult RefundTransaction(
		const FShopTransactionHandle& Transaction);

	// 모든 미확정 거래를 환불하며 실패한 거래는 원장에 남겨 재시도할 수 있게 한다.
	bool RefundAllActiveTransactions();

	/**
	 * Wallet을 변경하지 않고 모든 미확정 거래를 폐기한다.
	 * 새 경기에서 전체 경제를 초기화하기 직전에만 사용한다.
	 */
	bool DiscardAllActiveTransactionsForNewMatch();

	// 새 경기 초기화를 위해 거래 원장을 안전하게 폐기할 수 있는 상태인지 확인한다.
	bool CanDiscardAllActiveTransactionsForNewMatch(
		FString* OutFailureReason = nullptr) const;

	bool HasActiveTransaction(const FGuid& TransactionId) const;

	int32 GetActiveTransactionCount() const
	{
		return ActiveTransactions.Num();
	}

#if WITH_DEV_AUTOMATION_TESTS
	// 실제 GameState 페이즈를 구성하지 않는 자동화 테스트에서만 구매 가능 상태를 대체한다.
	void SetPurchaseAvailabilityOverrideForTesting(
		bool bEnabled,
		bool bShopOpen);
#endif

private:
	// 확정 구매 횟수가 소유 클라이언트에 복제됐을 때 UI 상태를 갱신한다.
	UFUNCTION()
	void OnRep_ConfirmedPurchaseCounts();

	// 구매 검증에서 조회한 상품, 가격, Wallet을 하나의 결과로 묶어 동일한 값을 후속 처리에 사용한다.
	struct FShopPurchaseResolution
	{
		EShopPurchaseResult Result = EShopPurchaseResult::InvalidItem;
		const FShopItemDefinition* Definition = nullptr;
		UCreditWalletComponent* Wallet = nullptr;
		int32 Price = 0;
	};

	FShopPurchaseResolution ResolvePurchaseRequest(
		FGameplayTag ItemId,
		int32 CreditOffset = 0) const;

	bool EnsureConfigurationValidated() const;
	const URogueShopSettings* GetShopSettings() const;
	const UShopCatalogDataAsset* GetShopCatalog() const;
	UCreditWalletComponent* GetCreditWallet() const;

	int32 GetConfirmedPurchaseCountForType(EShopItemType ItemType) const;

	int32 ResolvePrice(
		const FShopItemDefinition& Definition,
		int32 ConfirmedPurchaseCount) const;

	bool IsPurchaseWindowOpen(
		const URogueShopSettings& Settings) const;

	bool HasServerAuthority() const;

	EShopPurchaseResult ValidateActiveTransaction(
		const FShopTransactionHandle& Transaction,
		const FShopTransactionHandle*& OutActiveTransaction) const;

private:
	// 상점 설정은 런타임 중 바뀌지 않으므로 최초 검증 결과를 재사용한다.
	mutable TOptional<bool> CachedConfigurationValidation;

	// 결제됐지만 아직 확정 또는 환불되지 않은 서버 전용 거래 원장이다.
	TMap<FGuid, FShopTransactionHandle> ActiveTransactions;

	// 영구 스탯 강화의 확정 구매 횟수다.
	// PlayerState 소유 ShopComponent에 있으므로 라운드가 바뀌어도 유지된다.
	UPROPERTY(ReplicatedUsing = OnRep_ConfirmedPurchaseCounts)
	int32 HealthUpgradePurchaseCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ConfirmedPurchaseCounts)
	int32 DamageUpgradePurchaseCount = 0;

	// Wallet 변경 델리게이트 등으로 같은 거래 처리에 재진입하는 것을 차단한다.
	bool bIsProcessingTransaction = false;

#if WITH_DEV_AUTOMATION_TESTS
	bool bHasPurchaseAvailabilityOverrideForTesting = false;
	bool bPurchaseAvailabilityOverrideForTesting = false;
#endif
};
