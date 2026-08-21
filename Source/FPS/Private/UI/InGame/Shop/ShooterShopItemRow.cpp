#include "UI/InGame/Shop/ShooterShopItemRow.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UShooterShopItemRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(Button_Purchase))
	{
		Button_Purchase->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandlePurchaseClicked);
	}
}

void UShooterShopItemRow::SetItemDefinition(
	const FShopItemDefinition& Definition)
{
	ItemId = Definition.ItemId;

	if (IsValid(Text_Name))
	{
		Text_Name->SetText(Definition.DisplayName);
	}
}

void UShooterShopItemRow::RefreshPurchaseState(
	const int32 Price,
	const EShopPurchaseResult PurchaseResult)
{
	const bool bCanPurchase =
		PurchaseResult == EShopPurchaseResult::Success;

	if (IsValid(Button_Purchase))
	{
		Button_Purchase->SetIsEnabled(bCanPurchase);
	}

	if (IsValid(Text_Price))
	{
		if (PurchaseResult == EShopPurchaseResult::PurchaseLimitReached)
		{
			Text_Price->SetText(FText::FromString(TEXT("MAX")));
		}
		else if (Price > 0)
		{
			Text_Price->SetText(FText::AsNumber(Price));
		}
		else
		{
			Text_Price->SetText(FText::GetEmpty());
		}
	}

	if (!IsValid(Text_State))
	{
		return;
	}

	switch (PurchaseResult)
	{
	case EShopPurchaseResult::Success:
	case EShopPurchaseResult::NotEnoughCredits:
	case EShopPurchaseResult::PurchaseLimitReached:
		Text_State->SetText(FText::GetEmpty());
		break;

	default:
		Text_State->SetText(
			FText::FromString(TEXT("Unavailable")));
		break;
	}
}

void UShooterShopItemRow::HandlePurchaseClicked()
{
	if (!ItemId.IsValid())
	{
		return;
	}

	OnPurchaseRequested.Broadcast(ItemId);
}
