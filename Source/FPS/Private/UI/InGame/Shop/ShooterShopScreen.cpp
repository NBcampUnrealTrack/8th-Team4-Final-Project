#include "UI/InGame/Shop/ShooterShopScreen.h"

#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Economy/CreditWalletComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Shop/RogueShopSettings.h"
#include "Shop/ShopCatalogDataAsset.h"
#include "Shop/ShopComponent.h"
#include "Shop/ShopPurchaseComponent.h"
#include "UI/InGame/Shop/ShooterShopItemRow.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterShopScreen, Log, All);

UShooterShopScreen::UShooterShopScreen()
{
	InputConfig = EShooterWidgetInputMode::GameAndMenu;
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
	ClearItemPanels();

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
	const int32 Credits = IsValid(CreditWallet) ? CreditWallet->GetCredits() : 0;

	Text_Credits->SetText(FText::AsNumber(Credits));
}

void UShooterShopScreen::RebuildItemRows()
{
	if (!IsValid(Panel_PrimaryWeapons)
		|| !IsValid(Panel_Shields)
		|| !IsValid(Panel_Upgrades))
	{
		UE_LOG(
			LogShooterShopScreen,
			Error,
			TEXT("Shop item panels are not configured on %s."),
			*GetNameSafe(this));
		return;
	}

	ClearItemPanels();
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

	const UShopCatalogDataAsset* Catalog = Settings->LoadCatalog();
	if (!IsValid(Catalog))
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		return;
	}

	for (const FShopCatalogEntry& Entry : Catalog->Items)
	{
		const FShopItemDefinition& Definition = Entry.Definition;
		UPanelWidget* TargetPanel = GetTargetPanel(Definition);
		if (!IsValid(TargetPanel))
		{
			continue;
		}

		const FShopItemPresentation& Presentation = Entry.Presentation;

		TSubclassOf<UShooterShopItemRow> RowClass = ItemRowClass;
		if (!Presentation.WidgetClassOverride.IsNull())
		{
			if (UClass* OverrideClass = Presentation.WidgetClassOverride.LoadSynchronous())
			{
				RowClass = OverrideClass;
			}
			else
			{
				UE_LOG(
					LogShooterShopScreen,
					Warning,
					TEXT("Shop item %s could not load WidgetClassOverride. Falling back to the default row."),
					*Definition.ItemId.ToString());
			}
		}

		UShooterShopItemRow* ItemRow =
			CreateWidget<UShooterShopItemRow>(PlayerController, RowClass);

		if (!IsValid(ItemRow))
		{
			UE_LOG(
				LogShooterShopScreen,
				Error,
				TEXT("Failed to create shop item row for %s."),
				*Definition.ItemId.ToString());
			continue;
		}

		ItemRow->SetItemData(Definition, Presentation);
		ItemRow->OnPurchaseRequested.AddUObject(this, &ThisClass::HandleItemPurchaseRequested);

		UPanelSlot* AddedSlot = TargetPanel->AddChild(ItemRow);
		if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(AddedSlot))
		{
			FSlateChildSize FillSize;
			FillSize.SizeRule = ESlateSizeRule::Fill;
			FillSize.Value = 1.0f;

			HorizontalSlot->SetSize(FillSize);
			HorizontalSlot->SetHorizontalAlignment(HAlign_Fill);
			HorizontalSlot->SetVerticalAlignment(VAlign_Fill);
		}

		ItemRows.Add(ItemRow);
	}

	RefreshItemRows();
}

void UShooterShopScreen::RefreshItemRows()
{
	UShopPurchaseComponent* PurchaseComponent = BoundPurchaseComponent.Get();
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

void UShooterShopScreen::ClearItemPanels()
{
	if (IsValid(Panel_PrimaryWeapons))
	{
		Panel_PrimaryWeapons->ClearChildren();
	}

	if (IsValid(Panel_Shields))
	{
		Panel_Shields->ClearChildren();
	}

	if (IsValid(Panel_Upgrades))
	{
		Panel_Upgrades->ClearChildren();
	}
}

UPanelWidget* UShooterShopScreen::GetTargetPanel(const FShopItemDefinition& Definition) const
{
	switch (Definition.ItemType)
	{
	case EShopItemType::Weapon:
		return Panel_PrimaryWeapons;

	case EShopItemType::Shield:
		return Panel_Shields;

	case EShopItemType::WeaponUpgrade:
	case EShopItemType::HealthUpgrade:
	case EShopItemType::DamageUpgrade:
		return Panel_Upgrades;

	default:
		return nullptr;
	}
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

	UShopPurchaseComponent* PurchaseComponent = PlayerState->GetShopPurchaseComponent();
	if (!IsValid(PurchaseComponent))
	{
		return;
	}

	PurchaseComponent->ServerPurchaseItem(ItemId);

	// Listen Server에서는 RPC 처리가 즉시 끝날 수 있으므로 현재 상태를 한 번 더 반영한다.
	RefreshItemRows();
}

bool UShooterShopScreen::NativeOnHandleBackAction()
{
	RequestClose();
	return true;
}
