#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Economy/CreditWalletComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameMode/Types/RoundTypes.h"
#include "Player/ShooterPlayerState.h"
#include "Shop/RogueShopSettings.h"
#include "Shop/ShopCatalogDataAsset.h"
#include "Shop/ShopComponent.h"
#include "Shop/ShopTags.h"
#include "Shop/ShopTypes.h"
#include "Tags/GameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// 거래 테스트에서 사용하는 고정 가격이다. 실제 프로젝트 Catalog 값과는 독립적이다.
	constexpr int32 RiflePrice = 1500;
	constexpr int32 SmallShieldPrice = 150;
	constexpr int32 ExactBalanceCredits = RiflePrice;
	constexpr int32 SeedCredits = 2000;
	constexpr int32 MultiTransactionCredits = RiflePrice + SmallShieldPrice + 350;

	void TestRuleResult(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const FShopPurchaseContext& Context,
		const EShopPurchaseResult ExpectedResult)
	{
		Test.TestEqual(
			What,
			static_cast<int32>(ShopPurchaseRules::Evaluate(Context)),
			static_cast<int32>(ExpectedResult));
	}

	void TestPurchaseResult(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const EShopPurchaseResult ActualResult,
		const EShopPurchaseResult ExpectedResult)
	{
		Test.TestEqual(
			What,
			static_cast<int32>(ActualResult),
			static_cast<int32>(ExpectedResult));
	}


	struct FShopTestConfiguration
	{
		FShopTestConfiguration()
			: Catalog(NewObject<UShopCatalogDataAsset>(GetTransientPackage()))
			, Settings(NewObject<URogueShopSettings>(GetTransientPackage()))
		{
			FShopCatalogEntry Entry;
			Entry.Definition = FShopItemDefinition(
				RogueShopTags::Item_Weapon_Rifle,
				EShopItemType::Weapon,
				RiflePrice,
				RogueTags::Weapon_Type_Rifle);
			Entry.Presentation.DisplayName = FText::FromString(TEXT("Rifle"));
			Catalog->Items = { Entry };

			Settings->Catalog = Catalog.Get();
			Settings->AllowedPurchasePhases =
			{
				EGameFlowPhase::AugmentSelecting
			};
		}

		TStrongObjectPtr<UShopCatalogDataAsset> Catalog;
		TStrongObjectPtr<URogueShopSettings> Settings;
	};

	// 거래 테스트는 Content Asset에 의존하지 않고 필요한 최소 Catalog를 직접 구성한다.
	struct FScopedDefaultShopCatalog
	{
		FScopedDefaultShopCatalog()
			: Catalog(NewObject<UShopCatalogDataAsset>(GetTransientPackage()))
		{
			FShopCatalogEntry Rifle;
			Rifle.Definition = FShopItemDefinition(
				RogueShopTags::Item_Weapon_Rifle,
				EShopItemType::Weapon,
				RiflePrice,
				RogueTags::Weapon_Type_Rifle);

			FShopCatalogEntry SmallShield;
			SmallShield.Definition = FShopItemDefinition(
				RogueShopTags::Item_Shield_Small,
				EShopItemType::Shield,
				SmallShieldPrice);
			SmallShield.Definition.EffectClass = UGameplayEffect::StaticClass();

			Catalog->Items = { Rifle, SmallShield };

			Settings = GetMutableDefault<URogueShopSettings>();
			if (IsValid(Settings))
			{
				PreviousCatalog = Settings->Catalog;
				PreviousPurchasePhases = Settings->AllowedPurchasePhases;

				Settings->Catalog = Catalog.Get();
				Settings->AllowedPurchasePhases =
				{
					EGameFlowPhase::AugmentSelecting
				};
			}
		}

		~FScopedDefaultShopCatalog()
		{
			if (IsValid(Settings))
			{
				Settings->Catalog = PreviousCatalog;
				Settings->AllowedPurchasePhases = PreviousPurchasePhases;
			}
		}

		TStrongObjectPtr<UShopCatalogDataAsset> Catalog;
		TSoftObjectPtr<UShopCatalogDataAsset> PreviousCatalog;
		TArray<EGameFlowPhase> PreviousPurchasePhases;
		URogueShopSettings* Settings = nullptr;
	};

	void TestConfigurationRejected(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const URogueShopSettings& Settings)
	{
		FString FailureReason;
		Test.TestFalse(What, Settings.IsConfigurationValid(&FailureReason));
		Test.TestFalse(
			FString::Printf(TEXT("%s reports a failure reason"), What),
			FailureReason.IsEmpty());
	}

	/**
	 * 실제 거래를 돌리기 위한 최소 월드다.
	 * GameState 페이즈 대신 상점의 테스트 오버라이드로 구매 창을 연다.
	 */
	struct FShopTransactionFixture
	{
		FShopTransactionFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World)
			{
				return;
			}

			PlayerState = World->SpawnActor<AShooterPlayerState>();
			if (!IsValid(PlayerState))
			{
				return;
			}

			Shop = PlayerState->GetShopComponent();
			Wallet = PlayerState->GetCreditWallet();
		}

		~FShopTransactionFixture()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		FShopTransactionFixture(const FShopTransactionFixture&) = delete;
		FShopTransactionFixture& operator=(const FShopTransactionFixture&) = delete;

		bool IsReady() const
		{
			return IsValid(PlayerState) && IsValid(Shop) && IsValid(Wallet);
		}

		// 구매 창을 열고 지갑에 시드 잔액을 넣는다.
		bool OpenShopWithCredits(const int32 Credits) const
		{
			Shop->SetPurchaseAvailabilityOverrideForTesting(true, true);
			return Wallet->ResetCredits(Credits, ECreditChangeReason::InitialGrant);
		}

		FScopedDefaultShopCatalog CatalogOverride;
		UWorld* World = nullptr;
		AShooterPlayerState* PlayerState = nullptr;
		UShopComponent* Shop = nullptr;
		UCreditWalletComponent* Wallet = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShopDefaultCatalogTest,
	"FPS.Shop.Catalog.DefaultConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopDefaultCatalogTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const URogueShopSettings* Settings = GetDefault<URogueShopSettings>();
	if (!TestNotNull(TEXT("Shop settings exist"), Settings))
	{
		return false;
	}

	FString FailureReason;
	const bool bConfigurationValid = Settings->IsConfigurationValid(&FailureReason);
	TestTrue(
		FString::Printf(
			TEXT("Configured shop catalog is valid: %s"),
			FailureReason.IsEmpty() ? TEXT("<none>") : *FailureReason),
		bConfigurationValid);

	const UShopCatalogDataAsset* Catalog = Settings->LoadCatalog();
	TestNotNull(TEXT("Configured shop catalog loads"), Catalog);
	if (IsValid(Catalog))
	{
		TestFalse(TEXT("Configured shop catalog is not empty"), Catalog->Items.IsEmpty());
	}

	TestTrue(
		TEXT("Augment selection is an allowed purchase phase"),
		Settings->IsPurchasePhaseAllowed(EGameFlowPhase::AugmentSelecting));
	TestFalse(
		TEXT("Combat is not an allowed purchase phase"),
		Settings->IsPurchasePhaseAllowed(EGameFlowPhase::RoundInProgress));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShopCatalogFailureTest,
	"FPS.Shop.Catalog.InvalidConfigurationFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopCatalogFailureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TStrongObjectPtr<URogueShopSettings> MissingCatalog(
		NewObject<URogueShopSettings>(GetTransientPackage()));
	MissingCatalog->Catalog.Reset();
	TestConfigurationRejected(*this, TEXT("Missing catalog asset is rejected"), *MissingCatalog);

	FShopTestConfiguration EmptyCatalog;
	EmptyCatalog.Catalog->Items.Reset();
	TestConfigurationRejected(*this, TEXT("Empty catalog is rejected"), *EmptyCatalog.Settings);

	FShopTestConfiguration DuplicateItem;
	DuplicateItem.Catalog->Items.Add(DuplicateItem.Catalog->Items[0]);
	TestConfigurationRejected(*this, TEXT("Duplicate item id is rejected"), *DuplicateItem.Settings);

	FShopTestConfiguration InvalidPrice;
	InvalidPrice.Catalog->Items[0].Definition.BasePrice = 0;
	TestConfigurationRejected(*this, TEXT("Non-positive price is rejected"), *InvalidPrice.Settings);

	FShopTestConfiguration MissingWeaponTag;
	MissingWeaponTag.Catalog->Items[0].Definition.GrantedWeaponTag = FGameplayTag();
	TestConfigurationRejected(*this, TEXT("Weapon without a granted tag is rejected"), *MissingWeaponTag.Settings);

	FShopTestConfiguration InvalidPrimaryWeaponTag;
	InvalidPrimaryWeaponTag.Catalog->Items[0].Definition.GrantedWeaponTag = RogueTags::Weapon_Type_Pistol;
	TestConfigurationRejected(*this, TEXT("Primary weapon with a sidearm tag is rejected"), *InvalidPrimaryWeaponTag.Settings);

	FShopTestConfiguration InvalidWeaponUpgradeTag;
	InvalidWeaponUpgradeTag.Catalog->Items[0].Definition.ItemType = EShopItemType::WeaponUpgrade;
	TestConfigurationRejected(*this, TEXT("Weapon upgrade with a primary tag is rejected"), *InvalidWeaponUpgradeTag.Settings);

	FShopTestConfiguration MissingEffect;
	MissingEffect.Catalog->Items[0].Definition.ItemType = EShopItemType::Shield;
	MissingEffect.Catalog->Items[0].Definition.GrantedWeaponTag = FGameplayTag();
	TestConfigurationRejected(*this, TEXT("Effect-driven item without an effect is rejected"), *MissingEffect.Settings);

	FShopTestConfiguration InvalidTierPrice;
	InvalidTierPrice.Catalog->Items[0].Definition.TierPrices = { 1500, 0 };
	TestConfigurationRejected(*this, TEXT("Non-positive tier price is rejected"), *InvalidTierPrice.Settings);

	FShopTestConfiguration InvalidItemType;
	InvalidItemType.Catalog->Items[0].Definition.ItemType = static_cast<EShopItemType>(255);
	TestConfigurationRejected(*this, TEXT("Unknown item type is rejected"), *InvalidItemType.Settings);

	FShopTestConfiguration MissingPhase;
	MissingPhase.Settings->AllowedPurchasePhases.Reset();
	TestConfigurationRejected(*this, TEXT("Missing purchase phase is rejected"), *MissingPhase.Settings);

	FShopTestConfiguration DuplicatePhase;
	DuplicatePhase.Settings->AllowedPurchasePhases.Add(DuplicatePhase.Settings->AllowedPurchasePhases[0]);
	TestConfigurationRejected(*this, TEXT("Duplicate purchase phase is rejected"), *DuplicatePhase.Settings);

	FShopTestConfiguration InvalidPhase;
	InvalidPhase.Settings->AllowedPurchasePhases[0] = static_cast<EGameFlowPhase>(255);
	TestConfigurationRejected(*this, TEXT("Unknown purchase phase is rejected"), *InvalidPhase.Settings);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShopPurchaseRuleTest,
	"FPS.Shop.Rules.PurchaseValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopPurchaseRuleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FShopPurchaseContext Context;
	TestRuleResult(*this, TEXT("Invalid catalog fails closed"), Context, EShopPurchaseResult::InvalidCatalog);

	Context.bCatalogValid = true;
	TestRuleResult(*this, TEXT("Unknown item is rejected"), Context, EShopPurchaseResult::InvalidItem);

	Context.bItemFound = true;
	TestRuleResult(*this, TEXT("Non-positive price is rejected"), Context, EShopPurchaseResult::InvalidPrice);

	Context.Price = RiflePrice;
	TestRuleResult(*this, TEXT("Closed shop is rejected"), Context, EShopPurchaseResult::ShopClosed);

	Context.bShopOpen = true;
	TestRuleResult(*this, TEXT("Missing wallet is rejected"), Context, EShopPurchaseResult::WalletUnavailable);

	Context.bWalletAvailable = true;
	Context.CurrentCredits = RiflePrice - 1;
	TestRuleResult(*this, TEXT("Insufficient credits are rejected"), Context, EShopPurchaseResult::NotEnoughCredits);

	Context.CurrentCredits = RiflePrice;
	TestRuleResult(*this, TEXT("Exact balance is accepted"), Context, EShopPurchaseResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShopTransactionHandleValidationTest,
	"FPS.Shop.Transaction.HandleValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopTransactionHandleValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FShopTransactionHandle Transaction;
	TestFalse(TEXT("Empty transaction is invalid"), Transaction.IsValid());

	Transaction.TransactionId = FGuid::NewGuid();
	Transaction.ItemId = RogueShopTags::Item_Weapon_Rifle;
	Transaction.ItemType = EShopItemType::Weapon;
	Transaction.PaidCredits = RiflePrice;
	TestTrue(TEXT("Complete transaction is valid"), Transaction.IsValid());

	const FShopTransactionHandle ExactCopy = Transaction;
	TestTrue(TEXT("Exact transaction copy matches"), Transaction == ExactCopy);

	FShopTransactionHandle TamperedPrice = Transaction;
	TamperedPrice.PaidCredits = 1;
	TestFalse(TEXT("Modified price no longer matches"), Transaction == TamperedPrice);

	FShopTransactionHandle TamperedItem = Transaction;
	TamperedItem.ItemId = RogueShopTags::Item_Weapon_Sniper;
	TestFalse(TEXT("Modified item no longer matches"), Transaction == TamperedItem);

	FShopTransactionHandle MissingPrice = Transaction;
	MissingPrice.PaidCredits = 0;
	TestFalse(TEXT("Non-positive transaction price is invalid"), MissingPrice.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShopTransactionLifecycleTest,
	"FPS.Shop.Transaction.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopTransactionLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// 방어 로직을 의도적으로 호출하므로 예상되는 Warning을 테스트에 명시한다.
	AddExpectedError(
		TEXT("Transaction commit rejected"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	AddExpectedError(
		TEXT("Transaction refund rejected"),
		EAutomationExpectedErrorFlags::Contains,
		3);

	const FShopTransactionFixture Fixture;
	if (!TestTrue(TEXT("Shop transaction fixture is ready"), Fixture.IsReady()))
	{
		return false;
	}

	UShopComponent* Shop = Fixture.Shop;
	UCreditWalletComponent* Wallet = Fixture.Wallet;
	if (!TestTrue(TEXT("Wallet is seeded with the exact price"), Fixture.OpenShopWithCredits(ExactBalanceCredits)))
	{
		return false;
	}

	// 정확한 잔액 결제 -> 확정
	const FShopTransactionResult Purchase =
		Shop->TryBeginTransaction(RogueShopTags::Item_Weapon_Rifle);
	TestPurchaseResult(*this, TEXT("Purchase opens a transaction"), Purchase.Result, EShopPurchaseResult::Success);
	TestTrue(TEXT("Opened transaction is valid"), Purchase.Transaction.IsValid());
	TestEqual(TEXT("Purchase reports the exact balance before payment"), Purchase.CreditsBefore, ExactBalanceCredits);
	TestEqual(TEXT("Purchase reports zero after exact payment"), Purchase.CreditsAfter, 0);
	TestEqual(TEXT("Exact balance is debited exactly once"), Wallet->GetCredits(), 0);
	TestEqual(TEXT("Transaction is tracked"), Shop->GetActiveTransactionCount(), 1);
	TestTrue(TEXT("Transaction is queryable by id"), Shop->HasActiveTransaction(Purchase.Transaction.TransactionId));
	TestPurchaseResult(
		*this,
		TEXT("Commit preflight accepts the exact transaction"),
		Shop->CanCommitTransaction(Purchase.Transaction),
		EShopPurchaseResult::Success);
	FShopTransactionHandle MismatchedPreflight = Purchase.Transaction;
	MismatchedPreflight.PaidCredits = 1;
	TestPurchaseResult(
		*this,
		TEXT("Commit preflight rejects a modified transaction"),
		Shop->CanCommitTransaction(MismatchedPreflight),
		EShopPurchaseResult::TransactionMismatch);
	TestEqual(TEXT("Commit preflight does not close the transaction"), Shop->GetActiveTransactionCount(), 1);
	TestEqual(TEXT("Commit preflight does not move credits"), Wallet->GetCredits(), 0);

	TestPurchaseResult(*this, TEXT("Commit succeeds"), Shop->CommitTransaction(Purchase.Transaction), EShopPurchaseResult::Success);
	TestEqual(TEXT("Commit closes the transaction"), Shop->GetActiveTransactionCount(), 0);
	TestEqual(TEXT("Commit does not move credits"), Wallet->GetCredits(), 0);

	// 확정된 거래는 다시 확정하거나 환불할 수 없다.
	TestPurchaseResult(*this, TEXT("Double commit is rejected"), Shop->CommitTransaction(Purchase.Transaction), EShopPurchaseResult::InvalidTransaction);
	TestPurchaseResult(*this, TEXT("Refund after commit is rejected"), Shop->RefundTransaction(Purchase.Transaction), EShopPurchaseResult::InvalidTransaction);
	TestEqual(TEXT("Rejected replay leaves credits untouched"), Wallet->GetCredits(), 0);

	// 잔액 부족은 거래를 열지 않는다.
	const FShopTransactionResult Underfunded =
		Shop->TryBeginTransaction(RogueShopTags::Item_Weapon_Rifle);
	TestPurchaseResult(*this, TEXT("Insufficient credits are rejected"), Underfunded.Result, EShopPurchaseResult::NotEnoughCredits);
	TestEqual(TEXT("Rejected purchase opens no transaction"), Shop->GetActiveTransactionCount(), 0);
	TestEqual(TEXT("Rejected purchase leaves credits untouched"), Wallet->GetCredits(), 0);

	// 결제 -> 환불
	TestTrue(TEXT("Wallet is reseeded"), Wallet->ResetCredits(SeedCredits, ECreditChangeReason::NewMatchReset));
	const FShopTransactionResult Refundable =
		Shop->TryBeginTransaction(RogueShopTags::Item_Weapon_Rifle);
	TestPurchaseResult(*this, TEXT("Second purchase opens a transaction"), Refundable.Result, EShopPurchaseResult::Success);

	FShopTransactionHandle TamperedHandle = Refundable.Transaction;
	TamperedHandle.PaidCredits = 1;
	TestPurchaseResult(*this, TEXT("Tampered handle is rejected"), Shop->RefundTransaction(TamperedHandle), EShopPurchaseResult::TransactionMismatch);
	TestEqual(TEXT("Rejected refund leaves the transaction open"), Shop->GetActiveTransactionCount(), 1);
	TestEqual(TEXT("Rejected refund leaves credits untouched"), Wallet->GetCredits(), SeedCredits - RiflePrice);

	TestPurchaseResult(*this, TEXT("Refund succeeds"), Shop->RefundTransaction(Refundable.Transaction), EShopPurchaseResult::Success);
	TestEqual(TEXT("Refund restores the exact amount paid"), Wallet->GetCredits(), SeedCredits);
	TestEqual(TEXT("Refund closes the transaction"), Shop->GetActiveTransactionCount(), 0);
	TestPurchaseResult(*this, TEXT("Double refund is rejected"), Shop->RefundTransaction(Refundable.Transaction), EShopPurchaseResult::InvalidTransaction);
	TestEqual(TEXT("Double refund does not pay twice"), Wallet->GetCredits(), SeedCredits);

	// 구매 창이 닫히면 결제가 일어나지 않는다.
	Shop->SetPurchaseAvailabilityOverrideForTesting(true, false);
	const FShopTransactionResult ClosedShop =
		Shop->TryBeginTransaction(RogueShopTags::Item_Weapon_Rifle);
	TestPurchaseResult(*this, TEXT("Closed shop rejects purchases"), ClosedShop.Result, EShopPurchaseResult::ShopClosed);
	TestEqual(TEXT("Closed shop leaves credits untouched"), Wallet->GetCredits(), SeedCredits);
	TestEqual(TEXT("Closed shop opens no transaction"), Shop->GetActiveTransactionCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShopRefundAllTransactionsTest,
	"FPS.Shop.Transaction.RefundAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopRefundAllTransactionsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FShopTransactionFixture Fixture;
	if (!TestTrue(TEXT("Shop transaction fixture is ready"), Fixture.IsReady()))
	{
		return false;
	}

	UShopComponent* Shop = Fixture.Shop;
	UCreditWalletComponent* Wallet = Fixture.Wallet;
	if (!TestTrue(TEXT("Wallet is seeded"), Fixture.OpenShopWithCredits(MultiTransactionCredits)))
	{
		return false;
	}

	const FShopTransactionResult Rifle =
		Shop->TryBeginTransaction(RogueShopTags::Item_Weapon_Rifle);
	const FShopTransactionResult Shield =
		Shop->TryBeginTransaction(RogueShopTags::Item_Shield_Small);

	TestPurchaseResult(*this, TEXT("Rifle transaction opens"), Rifle.Result, EShopPurchaseResult::Success);
	TestPurchaseResult(*this, TEXT("Shield transaction opens"), Shield.Result, EShopPurchaseResult::Success);
	TestEqual(TEXT("Two transactions are tracked"), Shop->GetActiveTransactionCount(), 2);
	TestEqual(TEXT("Both payments are debited"), Wallet->GetCredits(), 350);

	TestTrue(TEXT("RefundAll succeeds"), Shop->RefundAllActiveTransactions());
	TestEqual(TEXT("RefundAll closes every transaction"), Shop->GetActiveTransactionCount(), 0);
	TestEqual(TEXT("RefundAll restores the exact total"), Wallet->GetCredits(), MultiTransactionCredits);
	TestTrue(TEXT("RefundAll is idempotent when the ledger is empty"), Shop->RefundAllActiveTransactions());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShopRefundFailureRecoveryTest,
	"FPS.Shop.Transaction.RefundFailureRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopRefundFailureRecoveryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	AddExpectedError(
		TEXT("Transaction refund failed"),
		EAutomationExpectedErrorFlags::Contains,
		1);

	const FShopTransactionFixture Fixture;
	if (!TestTrue(TEXT("Shop transaction fixture is ready"), Fixture.IsReady()))
	{
		return false;
	}

	UShopComponent* Shop = Fixture.Shop;
	UCreditWalletComponent* Wallet = Fixture.Wallet;
	if (!TestTrue(TEXT("Wallet is seeded"), Fixture.OpenShopWithCredits(SeedCredits)))
	{
		return false;
	}

	const FShopTransactionResult Purchase =
		Shop->TryBeginTransaction(RogueShopTags::Item_Weapon_Rifle);
	TestPurchaseResult(*this, TEXT("Purchase opens a transaction"), Purchase.Result, EShopPurchaseResult::Success);
	TestEqual(TEXT("Transaction is tracked before refund failure"), Shop->GetActiveTransactionCount(), 1);

	TestTrue(
		TEXT("Wallet is moved to max credits to force refund overflow"),
		Wallet->ResetCredits(MAX_int32, ECreditChangeReason::NewMatchReset));
	TestPurchaseResult(
		*this,
		TEXT("Overflowing refund is rejected"),
		Shop->RefundTransaction(Purchase.Transaction),
		EShopPurchaseResult::RefundFailed);
	TestEqual(TEXT("Failed refund leaves credits unchanged"), Wallet->GetCredits(), MAX_int32);
	TestEqual(TEXT("Failed refund restores the transaction to the ledger"), Shop->GetActiveTransactionCount(), 1);
	TestTrue(TEXT("Original transaction remains queryable"), Shop->HasActiveTransaction(Purchase.Transaction.TransactionId));

	TestTrue(
		TEXT("Wallet is lowered so the refund can be retried"),
		Wallet->ResetCredits(0, ECreditChangeReason::NewMatchReset));
	TestPurchaseResult(
		*this,
		TEXT("Refund retry succeeds"),
		Shop->RefundTransaction(Purchase.Transaction),
		EShopPurchaseResult::Success);
	TestEqual(TEXT("Refund retry restores the paid amount"), Wallet->GetCredits(), RiflePrice);
	TestEqual(TEXT("Refund retry closes the transaction"), Shop->GetActiveTransactionCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShopNewMatchDiscardTest,
	"FPS.Shop.Transaction.NewMatchDiscard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopNewMatchDiscardTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	AddExpectedError(
		TEXT("Transaction refund rejected"),
		EAutomationExpectedErrorFlags::Contains,
		1);

	const FShopTransactionFixture Fixture;
	if (!TestTrue(TEXT("Shop transaction fixture is ready"), Fixture.IsReady()))
	{
		return false;
	}

	UShopComponent* Shop = Fixture.Shop;
	UCreditWalletComponent* Wallet = Fixture.Wallet;
	if (!TestTrue(TEXT("Wallet is seeded"), Fixture.OpenShopWithCredits(SeedCredits)))
	{
		return false;
	}

	const FShopTransactionResult Purchase =
		Shop->TryBeginTransaction(RogueShopTags::Item_Weapon_Rifle);
	TestPurchaseResult(*this, TEXT("Purchase opens a transaction"), Purchase.Result, EShopPurchaseResult::Success);

	FString FailureReason;
	TestTrue(TEXT("Discard is allowed outside a transaction"), Shop->CanDiscardAllActiveTransactionsForNewMatch(&FailureReason));
	TestTrue(TEXT("Allowed discard reports no failure reason"), FailureReason.IsEmpty());
	TestTrue(TEXT("Discard succeeds"), Shop->DiscardAllActiveTransactionsForNewMatch());
	TestEqual(TEXT("Discard closes every transaction"), Shop->GetActiveTransactionCount(), 0);

	// 파기는 환불이 아니다. 잔액 복구는 뒤따르는 Wallet 초기화의 책임이다.
	TestEqual(TEXT("Discard does not refund"), Wallet->GetCredits(), SeedCredits - RiflePrice);
	TestPurchaseResult(*this, TEXT("Discarded transaction cannot be refunded"), Shop->RefundTransaction(Purchase.Transaction), EShopPurchaseResult::InvalidTransaction);
	TestEqual(TEXT("Rejected refund does not pay out"), Wallet->GetCredits(), SeedCredits - RiflePrice);
	return true;
}

#endif
