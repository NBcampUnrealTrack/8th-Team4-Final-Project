#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Weapon/WeaponTypes.h"

#include "ShopTypes.generated.h"

class UGameplayEffect;

UENUM(BlueprintType)
enum class EShopItemType : uint8
{
	None UMETA(DisplayName = "None"),
	Weapon UMETA(DisplayName = "Weapon"),
	WeaponUpgrade UMETA(DisplayName = "Weapon Upgrade"),
	HealthUpgrade UMETA(DisplayName = "Health Upgrade"),
	Shield UMETA(DisplayName = "Shield"),
	DamageUpgrade UMETA(DisplayName = "Damage Upgrade"),
	Augment UMETA(DisplayName = "Augment")
};

UENUM(BlueprintType)
enum class EShopPurchaseResult : uint8
{
	Success UMETA(DisplayName = "Success"),
	NotAuthority UMETA(DisplayName = "Server Authority Required"),
	ShopClosed UMETA(DisplayName = "Shop Closed"),
	InvalidCatalog UMETA(DisplayName = "Invalid Catalog"),
	InvalidItem UMETA(DisplayName = "Invalid Item"),
	InvalidPrice UMETA(DisplayName = "Invalid Price"),
	WalletUnavailable UMETA(DisplayName = "Wallet Unavailable"),
	PaymentFailed UMETA(DisplayName = "Payment Failed"),
	NotEnoughCredits UMETA(DisplayName = "Not Enough Credits"),
	TransactionInProgress UMETA(DisplayName = "Transaction In Progress"),
	InvalidTransaction UMETA(DisplayName = "Invalid Transaction"),
	TransactionMismatch UMETA(DisplayName = "Transaction Mismatch"),
	PurchaseLimitReached UMETA(DisplayName = "Purchase Limit Reached"),
	RefundFailed UMETA(DisplayName = "Refund Failed")
};

/** 서버 카탈로그가 소유하는 상품의 정적 정의다. 플레이어별 구매 상태는 포함하지 않는다. */
USTRUCT(BlueprintType)
struct FShopItemDefinition
{
	GENERATED_BODY()

	FShopItemDefinition() = default;

	FShopItemDefinition(
		const FGameplayTag InItemId,
		const EShopItemType InItemType,
		const int32 InBasePrice,
		const FText& InDisplayName,
		const EWeaponType InGrantedWeaponType = EWeaponType::None)
		: ItemId(InItemId)
		, ItemType(InItemType)
		, BasePrice(InBasePrice)
		, DisplayName(InDisplayName)
		, GrantedWeaponType(InGrantedWeaponType)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FGameplayTag ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	EShopItemType ItemType = EShopItemType::None;

	// 동적 가격 규칙이 추가되기 전의 기준 가격이다. 서버가 최종 결제 가격을 결정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "1", UIMin = "1"))
	int32 BasePrice = 1;

	// 반복 구매 상품의 단계별 가격이다.
	// 비어 있으면 BasePrice를 고정 가격으로 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TArray<int32> TierPrices;

	// 상품 적용에 사용할 GameplayEffect다. GE가 필요 없는 상품은 비워둔다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TSoftClassPtr<UGameplayEffect> EffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FText DisplayName;

	// 상점은 어떤 무기를 지급하는지만 정의한다. 실제 클래스 해석은 Weapon Registry가 담당한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	EWeaponType GrantedWeaponType = EWeaponType::None;
};

/** 결제 후 확정 또는 환불할 때 서버 C++ 코드에서만 사용하는 거래 핸들이다. */
struct FShopTransactionHandle
{
	bool IsValid() const
	{
		return TransactionId.IsValid()
			&& ItemId.IsValid()
			&& ItemType != EShopItemType::None
			&& PaidCredits > 0;
	}

	bool operator==(const FShopTransactionHandle& Other) const
	{
		return TransactionId == Other.TransactionId
			&& ItemId == Other.ItemId
			&& ItemType == Other.ItemType
			&& PaidCredits == Other.PaidCredits;
	}

	FGuid TransactionId;
	FGameplayTag ItemId;
	EShopItemType ItemType = EShopItemType::None;
	int32 PaidCredits = 0;
};

struct FShopPurchaseContext
{
	bool bCatalogValid = false;
	bool bItemFound = false;
	bool bShopOpen = false;
	bool bWalletAvailable = false;
	int32 Price = 0;
	int32 CurrentCredits = 0;
};

namespace ShopPurchaseRules
{
	FORCEINLINE EShopPurchaseResult Evaluate(const FShopPurchaseContext& Context)
	{
		if (!Context.bCatalogValid)
		{
			return EShopPurchaseResult::InvalidCatalog;
		}

		if (!Context.bItemFound)
		{
			return EShopPurchaseResult::InvalidItem;
		}

		if (Context.Price <= 0)
		{
			return EShopPurchaseResult::InvalidPrice;
		}

		if (!Context.bShopOpen)
		{
			return EShopPurchaseResult::ShopClosed;
		}

		if (!Context.bWalletAvailable)
		{
			return EShopPurchaseResult::WalletUnavailable;
		}

		if (Context.CurrentCredits < Context.Price)
		{
			return EShopPurchaseResult::NotEnoughCredits;
		}

		return EShopPurchaseResult::Success;
	}
}

struct FShopTransactionResult
{
	EShopPurchaseResult Result = EShopPurchaseResult::InvalidItem;
	FShopTransactionHandle Transaction;
	int32 CreditsBefore = 0;
	int32 CreditsAfter = 0;

	bool WasSuccessful() const
	{
		return Result == EShopPurchaseResult::Success && Transaction.IsValid();
	}
};
