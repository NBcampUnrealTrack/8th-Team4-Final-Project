#include "Shop/ShopPurchaseComponent.h"

#include "GAS/RogueAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Player/ShooterPlayerState.h"
#include "Shop/ShopComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogShopPurchase, Log, All);

UShopPurchaseComponent::UShopPurchaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UShopPurchaseComponent::ServerPurchaseItem_Implementation(const FGameplayTag ItemId)
{
	TryPurchaseItem(ItemId);
}

bool UShopPurchaseComponent::TryPurchaseItem(const FGameplayTag ItemId)
{
	AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(GetOwner());

	if (!PlayerState)
	{
		return false;
	}

	UShopComponent* ShopComponent = PlayerState->GetShopComponent();

	if (!ShopComponent)
	{
		return false;
	}

	FShopItemDefinition Definition;
	if (!ShopComponent->FindItemDefinition(ItemId, Definition))
	{
		return false;
	}

	// HP / Damage는 기획상 장바구니 대상이 아니며
	// 구매 성공 즉시 적용하고 거래를 확정한다.
	if (Definition.ItemType == EShopItemType::HealthUpgrade	|| Definition.ItemType == EShopItemType::DamageUpgrade)
	{
		return TryPurchaseImmediateItem(*ShopComponent,	Definition);
	}

	// Shield는 준비 페이즈 동안 Small/Large 교체가 가능하므로
	// 기존 Pending 거래 흐름을 사용한다.
	FShopTransactionHandle* PendingTransaction = GetPendingTransaction(Definition.ItemType);

	if (!PendingTransaction)
	{
		return false;
	}

	// 외부 정리 경로에서 원장이 먼저 닫힌 경우
	// 더 이상 유효하지 않은 로컬 핸들을 버린다.
	if (PendingTransaction->IsValid() && !ShopComponent->HasActiveTransaction(PendingTransaction->TransactionId))
	{
		*PendingTransaction = FShopTransactionHandle();
	}

	// 현재 Pending 상품과 같은 상품을 다시 클릭하면
	// 추가 결제나 GE 재적용을 하지 않는다.
	if (PendingTransaction->IsValid() && PendingTransaction->ItemId.MatchesTagExact(ItemId))
	{
		return true;
	}

	const FShopTransactionHandle PreviousTransaction = *PendingTransaction;

	const bool bReplacingExistingTransaction = PreviousTransaction.IsValid();

	const FShopTransactionResult TransactionResult = bReplacingExistingTransaction
		? ShopComponent->TryReplaceTransaction(PreviousTransaction, ItemId)
		: ShopComponent->TryBeginTransaction(ItemId);

	if (!TransactionResult.WasSuccessful())
	{
		UE_LOG(
			LogShopPurchase,
			Verbose,
			TEXT("Purchase rejected: Player=%s Item=%s Result=%s"),
			*GetNameSafe(PlayerState),
			*ItemId.ToString(),
			*UEnum::GetValueAsString(TransactionResult.Result));

		return false;
	}

	if (!ApplyPurchasedItem(Definition))
	{
		RollbackFailedApplication(
			*ShopComponent,
			PreviousTransaction,
			TransactionResult.Transaction,
			*PendingTransaction);

		return false;
	}

	*PendingTransaction = TransactionResult.Transaction;

	UE_LOG(
		LogShopPurchase,
		Log,
		TEXT("Purchase updated: Player=%s Item=%s Credits=%d->%d"),
		*GetNameSafe(PlayerState),
		*ItemId.ToString(),
		TransactionResult.CreditsBefore,
		TransactionResult.CreditsAfter);

	return true;
}

bool UShopPurchaseComponent::TryPurchaseImmediateItem(UShopComponent& ShopComponent, const FShopItemDefinition& Definition)
{
	const FShopTransactionResult TransactionResult = ShopComponent.TryBeginTransaction(Definition.ItemId);

	if (!TransactionResult.WasSuccessful())
	{
		UE_LOG(
			LogShopPurchase,
			Verbose,
			TEXT("Immediate purchase rejected: Player=%s Item=%s Result=%s"),
			*GetNameSafe(GetOwner()),
			*Definition.ItemId.ToString(),
			*UEnum::GetValueAsString(TransactionResult.Result));

		return false;
	}

	// 결제 후 실제 상품 적용.
	// 적용에 실패하면 Commit하지 않고 결제 금액을 환불한다.
	if (!ApplyPurchasedItem(Definition))
	{
		const EShopPurchaseResult RefundResult = ShopComponent.RefundTransaction(TransactionResult.Transaction);

		if (RefundResult != EShopPurchaseResult::Success)
		{
			UE_LOG(
				LogShopPurchase,
				Error,
				TEXT("Immediate purchase application failed and refund also failed: Player=%s Item=%s Result=%s"),
				*GetNameSafe(GetOwner()),
				*Definition.ItemId.ToString(),
				*UEnum::GetValueAsString(RefundResult));
		}

		return false;
	}

	// HP / Damage는 Pending으로 남기지 않고 즉시 확정한다.
	const EShopPurchaseResult CommitResult = ShopComponent.CommitTransaction(TransactionResult.Transaction);

	if (CommitResult != EShopPurchaseResult::Success)
	{
		UE_LOG(
			LogShopPurchase,
			Error,
			TEXT("Immediate purchase commit failed: Player=%s Item=%s Result=%s"),
			*GetNameSafe(GetOwner()),
			*Definition.ItemId.ToString(),
			*UEnum::GetValueAsString(CommitResult));

		return false;
	}

	UE_LOG(
		LogShopPurchase,
		Log,
		TEXT("Immediate purchase committed: Player=%s Item=%s Price=%d Credits=%d->%d"),
		*GetNameSafe(GetOwner()),
		*Definition.ItemId.ToString(),
		TransactionResult.Transaction.PaidCredits,
		TransactionResult.CreditsBefore,
		TransactionResult.CreditsAfter);

	return true;
}

FShopTransactionHandle*
UShopPurchaseComponent::GetPendingTransaction(const EShopItemType ItemType)
{
	switch (ItemType)
	{
	case EShopItemType::Shield:
		return &PendingShieldTransaction;

	default:
		return nullptr;
	}
}

bool UShopPurchaseComponent::ApplyPurchasedItem(const FShopItemDefinition& Definition)
{
	switch (Definition.ItemType)
	{
	case EShopItemType::Shield:
	case EShopItemType::HealthUpgrade:
	case EShopItemType::DamageUpgrade:
		return ApplyGameplayEffectPurchase(Definition);

	default:
		return false;
	}
}

bool UShopPurchaseComponent::ApplyGameplayEffectPurchase(const FShopItemDefinition& Definition)
{
	AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(GetOwner());

	if (!PlayerState)
	{
		return false;
	}

	URogueAbilitySystemComponent* AbilitySystem = PlayerState->GetRogueAbilitySystemComponent();

	if (!AbilitySystem)
	{
		return false;
	}

	if (Definition.EffectClass.IsNull())
	{
		return false;
	}

	UClass* EffectClass =
		Definition.EffectClass.LoadSynchronous();

	if (!EffectClass)
	{
		return false;
	}

	const UGameplayEffect* GameplayEffect = EffectClass->GetDefaultObject<UGameplayEffect>();

	if (!GameplayEffect)
	{
		return false;
	}

	const FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();

	const FActiveGameplayEffectHandle EffectHandle = AbilitySystem->ApplyGameplayEffectToSelf(
			GameplayEffect,
			1.f,
			EffectContext);

	if (!EffectHandle.WasSuccessfullyApplied())
	{
		UE_LOG(
			LogShopPurchase,
			Error,
			TEXT("Failed to apply shop GameplayEffect: Player=%s Item=%s Effect=%s"),
			*GetNameSafe(PlayerState),
			*Definition.ItemId.ToString(),
			*GetNameSafe(EffectClass));

		return false;
	}

	return true;
}

void UShopPurchaseComponent::RollbackFailedApplication(
	UShopComponent& ShopComponent,
	const FShopTransactionHandle& PreviousTransaction,
	const FShopTransactionHandle& CurrentTransaction,
	FShopTransactionHandle& PendingTransaction)
{
	// 기존 상품을 다른 상품으로 교체하던 중 적용이 실패했다면
	// 같은 거래를 이전 상품으로 다시 교체해 차액과 원장을 되돌린다.
	if (PreviousTransaction.IsValid())
	{
		const FShopTransactionResult RollbackResult = ShopComponent.TryReplaceTransaction(CurrentTransaction, PreviousTransaction.ItemId);

		if (RollbackResult.WasSuccessful())
		{
			PendingTransaction = RollbackResult.Transaction;
			return;
		}

		// 되돌리기에 실패했다면 실제 Shop 원장에 남은 현재 거래를 기준으로 보관한다.
		PendingTransaction = CurrentTransaction;

		UE_LOG(
			LogShopPurchase,
			Error,
			TEXT("Failed to rollback replaced purchase: CurrentItem=%s PreviousItem=%s"),
			*CurrentTransaction.ItemId.ToString(),
			*PreviousTransaction.ItemId.ToString());

		return;
	}

	// 최초 구매의 상품 적용이 실패했다면 결제를 전액 취소한다.
	const EShopPurchaseResult RefundResult = ShopComponent.RefundTransaction(CurrentTransaction);

	if (RefundResult == EShopPurchaseResult::Success)
	{
		PendingTransaction = FShopTransactionHandle();
		return;
	}

	// 환불 실패 시 거래는 ShopComponent 원장에 남으므로 핸들도 유지한다.
	PendingTransaction = CurrentTransaction;

	UE_LOG(
		LogShopPurchase,
		Error,
		TEXT("Failed to refund unapplied purchase: Item=%s Result=%s"),
		*CurrentTransaction.ItemId.ToString(),
		*UEnum::GetValueAsString(RefundResult));
}

bool UShopPurchaseComponent::CommitPendingPurchases()
{
	AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(GetOwner());

	if (!PlayerState)
	{
		return false;
	}

	UShopComponent* ShopComponent = PlayerState->GetShopComponent();

	if (!ShopComponent)
	{
		return false;
	}

	if (!PendingShieldTransaction.IsValid())
	{
		return true;
	}

	if (!ShopComponent->HasActiveTransaction(PendingShieldTransaction.TransactionId))
	{
		UE_LOG(
			LogShopPurchase,
			Error,
			TEXT("Pending shield transaction is missing from shop ledger: Player=%s Item=%s"),
			*GetNameSafe(PlayerState),
			*PendingShieldTransaction.ItemId.ToString());

		return false;
	}

	const EShopPurchaseResult CommitResult = ShopComponent->CommitTransaction(PendingShieldTransaction);

	if (CommitResult != EShopPurchaseResult::Success)
	{
		UE_LOG(
			LogShopPurchase,
			Error,
			TEXT("Pending purchase commit failed: Player=%s Item=%s Result=%s"),
			*GetNameSafe(PlayerState),
			*PendingShieldTransaction.ItemId.ToString(),
			*UEnum::GetValueAsString(CommitResult));

		return false;
	}

	UE_LOG(
		LogShopPurchase,
		Log,
		TEXT("Pending purchase committed: Player=%s Item=%s Price=%d"),
		*GetNameSafe(PlayerState),
		*PendingShieldTransaction.ItemId.ToString(),
		PendingShieldTransaction.PaidCredits);

	PendingShieldTransaction = FShopTransactionHandle();

	return true;
}