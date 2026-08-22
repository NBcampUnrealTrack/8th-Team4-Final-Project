#include "UI/InGame/Shop/ShooterShopScreen.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Economy/CreditWalletComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Shop/RogueShopSettings.h"
#include "Shop/ShopComponent.h"
#include "Shop/ShopPurchaseComponent.h"
#include "UI/InGame/Shop/ShooterShopItemRow.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterShopScreen, Log, All);

UShooterShopScreen::UShooterShopScreen()
{
	InputConfig = EShooterWidgetInputMode::GameAndMenu;

	// 현재 화면은 스탯 강화 상점으로 사용한다.
	// Blueprint Class Defaults에서 필요하면 표시 타입을 변경할 수 있다.
	VisibleItemTypes =
	{
		EShopItemType::HealthUpgrade,
		EShopItemType::Shield,
		EShopItemType::DamageUpgrade
	};
}

void UShooterShopScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	APlayerController* PlayerController = GetOwningPlayer();
	AShooterPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<AShooterPlayerState>()
		: nullptr;

	UCreditWalletComponent* CreditWallet = IsValid(PlayerState)
		? PlayerState->GetCreditWallet()
		: nullptr;

	UShopComponent* ShopComponent = IsValid(PlayerState)
		? PlayerState->GetShopComponent()
		: nullptr;

	UShopPurchaseComponent* PurchaseComponent = IsValid(PlayerState)
		? PlayerState->GetShopPurchaseComponent()
		: nullptr;

	BoundCreditWallet = CreditWallet;
	BoundShopComponent = ShopComponent;
	BoundPurchaseComponent = PurchaseComponent;

	if (IsValid(CreditWallet))
	{
		CreditWallet->OnCreditsChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleCreditsChanged);
	}

	if (IsValid(ShopComponent))
	{
		ShopComponent->OnPurchaseStateChanged.AddUObject(
			this,
			&ThisClass::RefreshItemRows);
	}

	if (IsValid(PurchaseComponent))
	{
		PurchaseComponent->OnPendingStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandlePendingStateChanged);
	}

	RefreshCredits();
	RebuildItemRows();
}

void UShooterShopScreen::NativeOnDeactivated()
{
	if (UCreditWalletComponent* CreditWallet = BoundCreditWallet.Get())
	{
		CreditWallet->OnCreditsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCreditsChanged);
	}

	if (UShopComponent* ShopComponent = BoundShopComponent.Get())
	{
		ShopComponent->OnPurchaseStateChanged.RemoveAll(this);
	}

	if (UShopPurchaseComponent* PurchaseComponent = BoundPurchaseComponent.Get())
	{
		PurchaseComponent->OnPendingStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePendingStateChanged);
	}

	for (UShooterShopItemRow* ItemRow : ItemRows)
	{
		if (IsValid(ItemRow))
		{
			ItemRow->OnPurchaseRequested.RemoveAll(this);
		}
	}

	ItemRows.Reset();

	if (IsValid(Panel_Items))
	{
		Panel_Items->ClearChildren();
	}

	BoundCreditWallet.Reset();
	BoundShopComponent.Reset();
	BoundPurchaseComponent.Reset();

	Super::NativeOnDeactivated();
}

void UShooterShopScreen::HandleCreditsChanged(const int32 PreviousCredits, const int32 NewCredits)
{
	(void)PreviousCredits;

	if (IsValid(Text_Credits))
	{
		Text_Credits->SetText(FText::AsNumber(NewCredits));
	}

	// 잔액 변화에 따라 구매 가능/불가 상태를 즉시 갱신한다.
	RefreshItemRows();
}

void UShooterShopScreen::HandlePendingStateChanged()
{
	RefreshItemRows();
}

void UShooterShopScreen::HandleItemPurchaseRequested(const FGameplayTag ItemId)
{
	RequestPurchase(ItemId);
}

void UShooterShopScreen::RefreshCredits()
{
	if (!IsValid(Text_Credits))
	{
		return;
	}

	const UCreditWalletComponent* CreditWallet = BoundCreditWallet.Get();

	const int32 Credits =
		IsValid(CreditWallet)
		? CreditWallet->GetCredits()
		: 0;

	Text_Credits->SetText(FText::AsNumber(Credits));
}

void UShooterShopScreen::RebuildItemRows()
{
	if (!IsValid(Panel_Items))
	{
		UE_LOG(
			LogShooterShopScreen,
			Error,
			TEXT("Panel_Items is missing on %s."),
			*GetNameSafe(this));
		return;
	}

	Panel_Items->ClearChildren();
	ItemRows.Reset();

	if (!ItemRowClass)
	{
		UE_LOG(
			LogShooterShopScreen,
			Error,
			TEXT("ItemRowClass is not configured on %s."),
			*GetNameSafe(this));
		return;
	}

	const URogueShopSettings* Settings = GetDefault<URogueShopSettings>();

	if (!IsValid(Settings) || !Settings->IsConfigurationValid())
	{
		UE_LOG(
			LogShooterShopScreen,
			Error,
			TEXT("Cannot build shop rows because Shop settings are invalid."));
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		return;
	}

	for (const FShopItemDefinition& Definition : Settings->ItemDefinitions)
	{
		if (!ShouldDisplayItem(Definition))
		{
			continue;
		}

		UShooterShopItemRow* ItemRow =
			CreateWidget<UShooterShopItemRow>(PlayerController, ItemRowClass);

		if (!IsValid(ItemRow))
		{
			UE_LOG(
				LogShooterShopScreen,
				Error,
				TEXT("Failed to create shop item row for %s."),
				*Definition.ItemId.ToString());
			continue;
		}

		ItemRow->SetItemDefinition(Definition);
		ItemRow->OnPurchaseRequested.AddUObject(this, &ThisClass::HandleItemPurchaseRequested);

		Panel_Items->AddChild(ItemRow);
		ItemRows.Add(ItemRow);
	}

	RefreshItemRows();
}

void UShooterShopScreen::RefreshItemRows()
{
	UShopPurchaseComponent* PurchaseComponent =
		BoundPurchaseComponent.Get();

	if (!IsValid(PurchaseComponent))
	{
		return;
	}

	for (UShooterShopItemRow* ItemRow : ItemRows)
	{
		if (!IsValid(ItemRow))
		{
			continue;
		}

		int32 Price = 0;
		const EShopPurchaseResult Result =
			PurchaseComponent->GetPurchaseEligibility(ItemRow->GetItemId(), Price);

		ItemRow->RefreshPurchaseState(Price, Result);
	}
}

bool UShooterShopScreen::ShouldDisplayItem(const FShopItemDefinition& Definition) const
{
	return VisibleItemTypes.IsEmpty() || VisibleItemTypes.Contains(Definition.ItemType);
}

void UShooterShopScreen::RequestClose()
{
	if (OnCloseRequested.IsBound())
	{
		OnCloseRequested.Broadcast();
		return;
	}

	DeactivateWidget();
}

void UShooterShopScreen::RequestPurchase(const FGameplayTag ItemId)
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		return;
	}

	AShooterPlayerState* PlayerState = PlayerController->GetPlayerState<AShooterPlayerState>();

	if (!IsValid(PlayerState))
	{
		return;
	}

	UShopPurchaseComponent* PurchaseComponent =	PlayerState->GetShopPurchaseComponent();

	if (!IsValid(PurchaseComponent))
	{
		return;
	}

	PurchaseComponent->ServerPurchaseItem(ItemId);

	// Listen Server에서는 RPC 처리가 즉시 끝날 수 있으므로
	// 현재 상태를 한 번 더 반영한다.
	RefreshItemRows();
}

bool UShooterShopScreen::NativeOnHandleBackAction()
{
	RequestClose();
	return true;
}