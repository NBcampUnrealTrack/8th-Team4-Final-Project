#include "Shop/ShopComponent.h"

#include "Economy/CreditWalletComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameMode/State/BaseRoundGameState.h"
#include "Player/ShooterPlayerState.h"
#include "Shop/RogueShopSettings.h"
#include "Shop/ShopCatalogDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogShop, Log, All);

UShopComponent::UShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UShopComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UShopComponent, HealthUpgradePurchaseCount, COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(UShopComponent, DamageUpgradePurchaseCount, COND_OwnerOnly);
}

void UShopComponent::OnRep_ConfirmedPurchaseCounts()
{
	OnPurchaseStateChanged.Broadcast();
}

void UShopComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(Cast<AShooterPlayerState>(GetOwner())))
	{
		UE_LOG(LogShop, Error,
			TEXT("ShopComponent must be owned by AShooterPlayerState. Owner=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	// 서버 인스턴스에서만 의존성과 카탈로그 설정을 진단해 클라이언트의 중복 로그를 막는다.
	if (!HasServerAuthority())
	{
		return;
	}

	if (!IsValid(GetCreditWallet()))
	{
		UE_LOG(LogShop, Error, TEXT("ShopComponent could not resolve CreditWallet for %s."), *GetNameSafe(GetOwner()));
	}

	EnsureConfigurationValidated();
}

void UShopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay에서는 수명주기 정리 중일 수 있으므로 Wallet을 변경하지 않는다.
	// 정상적인 접속 종료는 GameMode::Logout에서 PlayerState가 유효할 때 먼저 환불한다.
	if (!ActiveTransactions.IsEmpty())
	{
		UE_LOG(LogShop, Error,
			TEXT("Discarding %d unresolved shop transaction(s) during EndPlay without refunding. ")
			TEXT("Transactions must be closed before component teardown. Owner=%s Reason=%d"),
			ActiveTransactions.Num(),
			*GetNameSafe(GetOwner()),
			static_cast<int32>(EndPlayReason));
	}

	ActiveTransactions.Reset();
	bIsProcessingTransaction = false;

	Super::EndPlay(EndPlayReason);
}

bool UShopComponent::FindItemDefinition(const FGameplayTag ItemId, FShopItemDefinition& OutDefinition) const
{
	OutDefinition = FShopItemDefinition();

	if (!EnsureConfigurationValidated())
	{
		return false;
	}

	const UShopCatalogDataAsset* Catalog = GetShopCatalog();
	const FShopItemDefinition* Definition = IsValid(Catalog)
		? Catalog->FindItemDefinition(ItemId)
		: nullptr;

	if (!Definition)
	{
		return false;
	}

	OutDefinition = *Definition;
	return true;
}

EShopPurchaseResult UShopComponent::GetPurchaseEligibility(
	const FGameplayTag ItemId,
	int32& OutPrice,
	const int32 CreditOffset) const
{
	// UI 사전 조회와 실제 구매가 같은 검증 경로를 사용해 구매 조건의 불일치를 막는다.
	// 같은 함수를 쓰더라도 CreditOffset이 다르면 결과가 갈리므로 호출자가 맞춰 넘겨야 한다.
	const FShopPurchaseResolution Resolution = ResolvePurchaseRequest(ItemId, CreditOffset);

	OutPrice = Resolution.Price;
	return Resolution.Result;
}

int32 UShopComponent::GetConfirmedPurchaseCount(const FGameplayTag ItemId) const
{
	FShopItemDefinition Definition;
	if (!FindItemDefinition(ItemId, Definition))
	{
		return 0;
	}

	return GetConfirmedPurchaseCountForType(Definition.ItemType);
}

FShopTransactionResult UShopComponent::TryBeginTransaction(const FGameplayTag ItemId)
{
	FShopTransactionResult TransactionResult;

	if (!HasServerAuthority())
	{
		TransactionResult.Result = EShopPurchaseResult::NotAuthority;
		return TransactionResult;
	}

	if (bIsProcessingTransaction)
	{
		TransactionResult.Result = EShopPurchaseResult::TransactionInProgress;
		return TransactionResult;
	}

	// 결제 과정의 델리게이트 등에서 같은 상점으로 재진입해 중복 처리되는 것을 막는다.
	// 스코프를 벗어나면 기존 값으로 자동 복구된다.
	TGuardValue<bool> ProcessingGuard(bIsProcessingTransaction, true);

	const FShopPurchaseResolution Resolution = ResolvePurchaseRequest(ItemId);
	TransactionResult.Result = Resolution.Result;
	if (TransactionResult.Result != EShopPurchaseResult::Success)
	{
		UE_LOG(LogShop, Verbose,
			TEXT("Purchase rejected for %s: Item=%s Result=%s"),
			*GetNameSafe(GetOwner()),
			*ItemId.ToString(),
			*UEnum::GetValueAsString(TransactionResult.Result));
		return TransactionResult;
	}

	const FShopItemDefinition& Definition = *Resolution.Definition;
	UCreditWalletComponent* Wallet = Resolution.Wallet;
	const int32 Price = Resolution.Price;

	// 서버 원장에서 사용할 유효하고 중복되지 않은 거래 ID를 생성한다.
	FShopTransactionHandle Transaction;
	do
	{
		Transaction.TransactionId = FGuid::NewGuid();
	}
	while (!Transaction.TransactionId.IsValid() || ActiveTransactions.Contains(Transaction.TransactionId));

	Transaction.ItemId = Definition.ItemId;
	Transaction.ItemType = Definition.ItemType;
	Transaction.PaidCredits = Price;

	if (!Transaction.IsValid())
	{
		TransactionResult.Result = EShopPurchaseResult::InvalidTransaction;
		return TransactionResult;
	}

	// 결제 델리게이트가 발생해도 이미 열린 거래가 원장에 보이도록 결제 전에 등록한다.
	ActiveTransactions.Add(Transaction.TransactionId, Transaction);

	TransactionResult.CreditsBefore = Wallet->GetCredits();
	if (!Wallet->TrySpendCredits(Price, ECreditChangeReason::Purchase))
	{
		ActiveTransactions.Remove(Transaction.TransactionId);

		TransactionResult.CreditsAfter = Wallet->GetCredits();
		TransactionResult.Result = Wallet->CanAfford(Price)	? EShopPurchaseResult::PaymentFailed: EShopPurchaseResult::NotEnoughCredits;

		if (TransactionResult.Result == EShopPurchaseResult::PaymentFailed)
		{
			UE_LOG(LogShop, Error,
				TEXT("Payment failed for %s after validation: Item=%s Price=%d Credits=%d"),
				*GetNameSafe(GetOwner()),
				*ItemId.ToString(),
				Price,
				TransactionResult.CreditsAfter);
		}
		else
		{
			UE_LOG(LogShop, Verbose, TEXT("Payment rejected for %s: Item=%s Price=%d Credits=%d Result=%s"),
				*GetNameSafe(GetOwner()),
				*ItemId.ToString(),
				Price,
				TransactionResult.CreditsAfter,
				*UEnum::GetValueAsString(TransactionResult.Result));
		}
		return TransactionResult;
	}

	TransactionResult.Transaction = Transaction;
	TransactionResult.CreditsAfter = Wallet->GetCredits();
	TransactionResult.Result = EShopPurchaseResult::Success;

	UE_LOG(LogShop, Verbose,
		TEXT("Transaction opened for %s: Id=%s Item=%s Price=%d Credits=%d->%d"),
		*GetNameSafe(GetOwner()),
		*Transaction.TransactionId.ToString(EGuidFormats::DigitsWithHyphensLower),
		*Transaction.ItemId.ToString(),
		Transaction.PaidCredits,
		TransactionResult.CreditsBefore,
		TransactionResult.CreditsAfter);
	return TransactionResult;
}

FShopTransactionResult UShopComponent::TryReplaceTransaction(
	const FShopTransactionHandle& CurrentTransaction,
	const FGameplayTag NewItemId)
{
	FShopTransactionResult TransactionResult;

	if (!HasServerAuthority())
	{
		TransactionResult.Result = EShopPurchaseResult::NotAuthority;
		return TransactionResult;
	}

	if (bIsProcessingTransaction)
	{
		TransactionResult.Result = EShopPurchaseResult::TransactionInProgress;
		return TransactionResult;
	}

	TGuardValue<bool> ProcessingGuard(bIsProcessingTransaction, true);

	const FShopTransactionHandle* ActiveTransaction = nullptr;
	const EShopPurchaseResult ValidationResult = ValidateActiveTransaction(CurrentTransaction, ActiveTransaction);

	if (ValidationResult != EShopPurchaseResult::Success)
	{
		TransactionResult.Result = ValidationResult;
		return TransactionResult;
	}

	// Map 갱신 이후에도 안전하게 사용할 수 있도록 서버 원본을 복사한다.
	const FShopTransactionHandle PreviousTransaction = *ActiveTransaction;

	const FShopPurchaseResolution Resolution = ResolvePurchaseRequest(NewItemId, PreviousTransaction.PaidCredits);

	if (Resolution.Result != EShopPurchaseResult::Success)
	{
		TransactionResult.Result = Resolution.Result;
		return TransactionResult;
	}

	if (!Resolution.Definition || !IsValid(Resolution.Wallet))
	{
		TransactionResult.Result = EShopPurchaseResult::InvalidItem;
		return TransactionResult;
	}

	// 같은 종류의 상품끼리만 교체한다.
	if (Resolution.Definition->ItemType != PreviousTransaction.ItemType)
	{
		TransactionResult.Result = EShopPurchaseResult::InvalidItem;
		return TransactionResult;
	}

	UCreditWalletComponent* Wallet = Resolution.Wallet;

	TransactionResult.CreditsBefore = Wallet->GetCredits();

	// 같은 상품이면 아무것도 변경하지 않는다.
	if (PreviousTransaction.ItemId.MatchesTagExact(Resolution.Definition->ItemId))
	{
		TransactionResult.Transaction = PreviousTransaction;
		TransactionResult.CreditsAfter = TransactionResult.CreditsBefore;
		TransactionResult.Result = EShopPurchaseResult::Success;
		return TransactionResult;
	}

	FShopTransactionHandle ReplacementTransaction;
	ReplacementTransaction.TransactionId = PreviousTransaction.TransactionId;
	ReplacementTransaction.ItemId = Resolution.Definition->ItemId;
	ReplacementTransaction.ItemType = Resolution.Definition->ItemType;
	ReplacementTransaction.PaidCredits = Resolution.Price;

	if (!ReplacementTransaction.IsValid())
	{
		TransactionResult.CreditsAfter = TransactionResult.CreditsBefore;
		TransactionResult.Result = EShopPurchaseResult::InvalidTransaction;
		return TransactionResult;
	}

	const int64 PriceDelta = static_cast<int64>(ReplacementTransaction.PaidCredits) - static_cast<int64>(PreviousTransaction.PaidCredits);

	// Wallet 변경 이벤트가 발생했을 때도 새 거래 상태가 보이도록
	// 먼저 서버 원장을 교체한다.
	ActiveTransactions.Add(ReplacementTransaction.TransactionId, ReplacementTransaction);

	auto RestorePreviousTransaction = [this, &PreviousTransaction]()
		{
			ActiveTransactions.Add(PreviousTransaction.TransactionId, PreviousTransaction);
		};

	if (PriceDelta > 0)
	{
		const int32 AdditionalCost = static_cast<int32>(PriceDelta);

		if (!Wallet->TrySpendCredits(AdditionalCost, ECreditChangeReason::Purchase))
		{
			RestorePreviousTransaction();

			TransactionResult.CreditsAfter = Wallet->GetCredits();
			TransactionResult.Result =
				Wallet->CanAfford(AdditionalCost) ? EShopPurchaseResult::PaymentFailed : EShopPurchaseResult::NotEnoughCredits;

			return TransactionResult;
		}
	}
	else if (PriceDelta < 0)
	{
		const int32 RefundAmount = static_cast<int32>(-PriceDelta);

		if (!Wallet->AddCredits(RefundAmount, ECreditChangeReason::PurchaseRefund))
		{
			RestorePreviousTransaction();

			TransactionResult.CreditsAfter = Wallet->GetCredits();
			TransactionResult.Result = EShopPurchaseResult::RefundFailed;
			return TransactionResult;
		}
	}

	TransactionResult.Transaction = ReplacementTransaction;
	TransactionResult.CreditsAfter = Wallet->GetCredits();
	TransactionResult.Result = EShopPurchaseResult::Success;

	UE_LOG(
		LogShop,
		Verbose,
		TEXT("Transaction replaced for %s: Id=%s PreviousItem=%s NewItem=%s PreviousPrice=%d NewPrice=%d Credits=%d->%d"),
		*GetNameSafe(GetOwner()),
		*ReplacementTransaction.TransactionId.ToString(
			EGuidFormats::DigitsWithHyphensLower),
		*PreviousTransaction.ItemId.ToString(),
		*ReplacementTransaction.ItemId.ToString(),
		PreviousTransaction.PaidCredits,
		ReplacementTransaction.PaidCredits,
		TransactionResult.CreditsBefore,
		TransactionResult.CreditsAfter);

	return TransactionResult;
}

EShopPurchaseResult UShopComponent::CanCommitTransaction(
	const FShopTransactionHandle& Transaction) const
{
	if (!HasServerAuthority())
	{
		return EShopPurchaseResult::NotAuthority;
	}

	if (bIsProcessingTransaction)
	{
		return EShopPurchaseResult::TransactionInProgress;
	}

	const FShopTransactionHandle* ActiveTransaction = nullptr;
	return ValidateActiveTransaction(Transaction, ActiveTransaction);
}

EShopPurchaseResult UShopComponent::CommitTransaction(const FShopTransactionHandle& Transaction)
{
	const EShopPurchaseResult ValidationResult = CanCommitTransaction(Transaction);
	if (ValidationResult != EShopPurchaseResult::Success)
	{
		UE_LOG(LogShop, Warning,
			TEXT("Transaction commit rejected for %s: Id=%s Result=%s"),
			*GetNameSafe(GetOwner()),
			*Transaction.TransactionId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*UEnum::GetValueAsString(ValidationResult));
		return ValidationResult;
	}

	TGuardValue<bool> ProcessingGuard(bIsProcessingTransaction, true);

	// 실제 상품 적용은 호출자가 완료하고 Commit은 미확정 거래를 원장에서 닫는 역할만 한다.
	// 사전검증에서 서버 원장과 전체 필드가 일치했으므로 검증된 요청 값을 그대로 사용한다.
	const FShopTransactionHandle CommittedTransaction = Transaction;

	bool bPurchaseCountChanged = false;

	switch (CommittedTransaction.ItemType)
	{
	case EShopItemType::HealthUpgrade:
		++HealthUpgradePurchaseCount;
		bPurchaseCountChanged = true;
		break;

	case EShopItemType::DamageUpgrade:
		++DamageUpgradePurchaseCount;
		bPurchaseCountChanged = true;
		break;

	default:
		break;
	}

	ActiveTransactions.Remove(CommittedTransaction.TransactionId);

	if (bPurchaseCountChanged)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->ForceNetUpdate();
		}
	}

	UE_LOG(LogShop, Verbose,
		TEXT("Transaction committed for %s: Id=%s Item=%s Price=%d"),
		*GetNameSafe(GetOwner()),
		*CommittedTransaction.TransactionId.ToString(EGuidFormats::DigitsWithHyphensLower),
		*CommittedTransaction.ItemId.ToString(),
		CommittedTransaction.PaidCredits);
	return EShopPurchaseResult::Success;
}

EShopPurchaseResult UShopComponent::RefundTransaction(const FShopTransactionHandle& Transaction)
{
	if (!HasServerAuthority())
	{
		return EShopPurchaseResult::NotAuthority;
	}

	if (bIsProcessingTransaction)
	{
		return EShopPurchaseResult::TransactionInProgress;
	}

	TGuardValue<bool> ProcessingGuard(bIsProcessingTransaction, true);

	const FShopTransactionHandle* ActiveTransaction = nullptr;
	const EShopPurchaseResult ValidationResult = ValidateActiveTransaction(Transaction, ActiveTransaction);
	if (ValidationResult != EShopPurchaseResult::Success)
	{
		UE_LOG(LogShop, Warning, TEXT("Transaction refund rejected for %s: Id=%s Result=%s"),
			*GetNameSafe(GetOwner()),
			*Transaction.TransactionId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*UEnum::GetValueAsString(ValidationResult));
		return ValidationResult;
	}

	UCreditWalletComponent* Wallet = GetCreditWallet();
	if (!IsValid(Wallet))
	{
		return EShopPurchaseResult::WalletUnavailable;
	}

	// 환불 금액은 요청 값이 아니라 서버 원장에 보관된 최초 결제 금액을 사용한다.
	const FShopTransactionHandle RefundedTransaction = *ActiveTransaction;
	ActiveTransaction = nullptr;

	// 잔액 변경 델리게이트가 이미 닫힌 거래를 보도록 지급 전에 제거한다.
	ActiveTransactions.Remove(RefundedTransaction.TransactionId);

	const int32 CreditsBefore = Wallet->GetCredits();
	if (!Wallet->AddCredits(RefundedTransaction.PaidCredits, ECreditChangeReason::PurchaseRefund))
	{
		// 환불이 실패하면 거래를 복구해 이후 다시 환불할 수 있는 상태로 남긴다.
		ActiveTransactions.Add(RefundedTransaction.TransactionId, RefundedTransaction);

		UE_LOG(LogShop, Error,
			TEXT("Transaction refund failed for %s: Id=%s Item=%s Price=%d"),
			*GetNameSafe(GetOwner()),
			*RefundedTransaction.TransactionId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*RefundedTransaction.ItemId.ToString(),
			RefundedTransaction.PaidCredits);
		return EShopPurchaseResult::RefundFailed;
	}

	UE_LOG(LogShop, Verbose,
		TEXT("Transaction refunded for %s: Id=%s Item=%s Credits=%d->%d"),
		*GetNameSafe(GetOwner()),
		*RefundedTransaction.TransactionId.ToString(EGuidFormats::DigitsWithHyphensLower),
		*RefundedTransaction.ItemId.ToString(),
		CreditsBefore,
		Wallet->GetCredits());
	return EShopPurchaseResult::Success;
}

bool UShopComponent::RefundAllActiveTransactions()
{
	if (!HasServerAuthority())
	{
		return false;
	}

	// 개별 환불이 각자 재진입 가드를 사용하므로 여기서는 진입 시점만 확인한다.
	if (bIsProcessingTransaction)
	{
		UE_LOG(LogShop, Warning, TEXT("RefundAllActiveTransactions rejected during an active transaction. Owner=%s"),
			*GetNameSafe(GetOwner()));
		return false;
	}

	// RefundTransaction이 원본 Map을 수정하므로 현재 거래 목록을 복사한 뒤 순회한다.
	TArray<FShopTransactionHandle> Transactions;
	ActiveTransactions.GenerateValueArray(Transactions);

	bool bAllRefunded = true;
	for (const FShopTransactionHandle& Transaction : Transactions)
	{
		bAllRefunded &= RefundTransaction(Transaction) == EShopPurchaseResult::Success;
	}
	return bAllRefunded;
}

bool UShopComponent::CanDiscardAllActiveTransactionsForNewMatch(FString* OutFailureReason) const
{
	// 새 경기 경제 초기화 전에 거래 원장을 안전하게 폐기할 수 있는 상태인지 먼저 검증한다.
	auto Fail = [OutFailureReason](const FString& FailureReason)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FailureReason;
			}
			return false;
		};

	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return Fail(TEXT("server authority is required"));
	}

	if (bIsProcessingTransaction)
	{
		return Fail(TEXT("a shop transaction is in progress"));
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}
	return true;
}

bool UShopComponent::DiscardAllActiveTransactionsForNewMatch()
{
	FString FailureReason;
	if (!CanDiscardAllActiveTransactionsForNewMatch(&FailureReason))
	{
		UE_LOG(LogShop, Warning,
			TEXT("DiscardAllActiveTransactionsForNewMatch rejected for %s: %s"),
			*GetNameSafe(GetOwner()),
			*FailureReason);
		return false;
	}

	// 새 경기는 Wallet 자체를 초기화하므로 개별 환불하지 않고 기존 미확정 거래 원장만 폐기한다.
	const int32 DiscardedCount = ActiveTransactions.Num();
	ActiveTransactions.Reset();

	UE_LOG(LogShop, Verbose,
		TEXT("Discarded %d unresolved shop transaction(s) for new-match economy reset. Owner=%s"),
		DiscardedCount,
		*GetNameSafe(GetOwner()));

	return true;
}

bool UShopComponent::HasActiveTransaction(const FGuid& TransactionId) const
{
	return TransactionId.IsValid() && ActiveTransactions.Contains(TransactionId);
}

UShopComponent::FShopPurchaseResolution
UShopComponent::ResolvePurchaseRequest(const FGameplayTag ItemId, const int32 CreditOffset) const
{
	// 구매 가능 조회와 실제 거래가 동일한 상품, 가격, 페이즈, Wallet 검증 경로를 사용하도록 한곳에서 해석한다.
	FShopPurchaseResolution Resolution;

	const URogueShopSettings* Settings = GetShopSettings();
	const bool bConfigurationValid = EnsureConfigurationValidated();
	const UShopCatalogDataAsset* Catalog = bConfigurationValid ? GetShopCatalog() : nullptr;
	Resolution.Definition = IsValid(Catalog) ? Catalog->FindItemDefinition(ItemId) : nullptr;
	Resolution.Wallet = GetCreditWallet();

	if (Resolution.Definition)
	{
		const FShopItemDefinition& Definition = *Resolution.Definition;
		const int32 PurchaseCount = GetConfirmedPurchaseCountForType(Definition.ItemType);

		if (!Definition.TierPrices.IsEmpty()
			&& PurchaseCount >= Definition.TierPrices.Num())
		{
			Resolution.Result = EShopPurchaseResult::PurchaseLimitReached;
			return Resolution;
		}

		Resolution.Price = ResolvePrice(Definition, PurchaseCount);
	}

	FShopPurchaseContext Context;
	Context.bCatalogValid = bConfigurationValid;
	Context.bItemFound = Resolution.Definition != nullptr;
	Context.Price = Resolution.Price;
	Context.bShopOpen = bConfigurationValid && IsPurchaseWindowOpen(*Settings);
	Context.bWalletAvailable = IsValid(Resolution.Wallet);
	if (IsValid(Resolution.Wallet))
	{
		const int64 AvailableCredits = static_cast<int64>(Resolution.Wallet->GetCredits()) + FMath::Max(0, CreditOffset);

		Context.CurrentCredits = static_cast<int32>(FMath::Min<int64>(AvailableCredits, MAX_int32));
	}
	else
	{
		Context.CurrentCredits = 0;
	}

	Resolution.Result = ShopPurchaseRules::Evaluate(Context);
	return Resolution;
}

bool UShopComponent::EnsureConfigurationValidated() const
{
	if (CachedConfigurationValidation.IsSet())
	{
		return CachedConfigurationValidation.GetValue();
	}

	const URogueShopSettings* Settings = GetShopSettings();
	FString FailureReason;
	const bool bValid = IsValid(Settings) && Settings->IsConfigurationValid(&FailureReason);
	CachedConfigurationValidation = bValid;

	if (!bValid && HasServerAuthority())
	{
		UE_LOG(LogShop, Error,
			TEXT("Shop configuration is invalid for %s: %s"),
			*GetNameSafe(GetOwner()),
			FailureReason.IsEmpty() ? TEXT("Settings unavailable") : *FailureReason);
	}

	return bValid;
}

const URogueShopSettings* UShopComponent::GetShopSettings() const
{
	return GetDefault<URogueShopSettings>();
}

const UShopCatalogDataAsset* UShopComponent::GetShopCatalog() const
{
	const URogueShopSettings* Settings = GetShopSettings();
	return IsValid(Settings) ? Settings->LoadCatalog() : nullptr;
}

UCreditWalletComponent* UShopComponent::GetCreditWallet() const
{
	const AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(GetOwner());
	return IsValid(PlayerState) ? PlayerState->GetCreditWallet() : nullptr;
}

int32 UShopComponent::GetConfirmedPurchaseCountForType(const EShopItemType ItemType) const
{
	switch (ItemType)
	{
	case EShopItemType::HealthUpgrade:
		return HealthUpgradePurchaseCount;

	case EShopItemType::DamageUpgrade:
		return DamageUpgradePurchaseCount;

	default:
		return 0;
	}
}

int32 UShopComponent::ResolvePrice(
	const FShopItemDefinition& Definition,
	const int32 ConfirmedPurchaseCount) const
{
	if (Definition.TierPrices.IsEmpty())
	{
		return Definition.BasePrice;
	}

	return Definition.TierPrices.IsValidIndex(ConfirmedPurchaseCount)
		? Definition.TierPrices[ConfirmedPurchaseCount]
		: 0;
}

bool UShopComponent::IsPurchaseWindowOpen() const
{
	const URogueShopSettings* Settings = GetShopSettings();
	return IsValid(Settings) && IsPurchaseWindowOpen(*Settings);
}

bool UShopComponent::IsPurchaseWindowOpen(const URogueShopSettings& Settings) const
{
	const UWorld* World = GetWorld();
	const ABaseRoundGameState* RoundGameState = IsValid(World) ? World->GetGameState<ABaseRoundGameState>() : nullptr;
	return IsValid(RoundGameState) && Settings.IsPurchasePhaseAllowed(RoundGameState->GetGameFlowPhase());
}

bool UShopComponent::HasServerAuthority() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority();
}

EShopPurchaseResult UShopComponent::ValidateActiveTransaction(
	const FShopTransactionHandle& Transaction,
	const FShopTransactionHandle*& OutActiveTransaction) const
{
	OutActiveTransaction = nullptr;
	if (!Transaction.IsValid())
	{
		return EShopPurchaseResult::InvalidTransaction;
	}

	OutActiveTransaction = ActiveTransactions.Find(Transaction.TransactionId);
	if (!OutActiveTransaction)
	{
		return EShopPurchaseResult::InvalidTransaction;
	}

	// ID만 일치하는 변조된 거래를 허용하지 않고 서버가 보관한 원본 거래 전체와 대조한다.
	if (!(*OutActiveTransaction == Transaction))
	{
		OutActiveTransaction = nullptr;
		return EShopPurchaseResult::TransactionMismatch;
	}

	return EShopPurchaseResult::Success;
}
