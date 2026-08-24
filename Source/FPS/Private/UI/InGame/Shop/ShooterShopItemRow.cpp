#include "UI/InGame/Shop/ShooterShopItemRow.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UShooterShopItemRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(Button_Purchase))
	{
		Button_Purchase->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandlePurchaseClicked);
	}

	if (IsValid(Button_Refund))
	{
		Button_Refund->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleRefundClicked);
	}
}

void UShooterShopItemRow::SetItemData(
	const FShopItemDefinition& Definition,
	const FShopItemPresentation& Presentation)
{
	ItemId = Definition.ItemId;

	if (IsValid(Text_Name))
	{
		Text_Name->SetText(Presentation.DisplayName);
	}

	if (IsValid(Image_Icon))
	{
		if (UTexture2D* IconTexture = Presentation.Icon.LoadSynchronous())
		{
			Image_Icon->SetBrushFromTexture(IconTexture, true);
			Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UShooterShopItemRow::RefreshPurchaseState(
	const int32 Price,
	const EShopPurchaseResult PurchaseResult,
	const bool bIsPending,
	const bool bIsEquippedPending)
{
	const bool bCanPurchase = PurchaseResult == EShopPurchaseResult::Success;

	if (IsValid(Button_Purchase))
	{
		Button_Purchase->SetIsEnabled(bCanPurchase);
	}

	if (IsValid(Button_Refund))
	{
		Button_Refund->SetVisibility(
			bIsPending ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_Refund->SetIsEnabled(bIsPending);
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

	if (bIsEquippedPending)
	{
		Text_State->SetText(NSLOCTEXT("ShooterShop", "EquippedPending", "EQUIPPED"));
		return;
	}

	if (bIsPending)
	{
		Text_State->SetText(NSLOCTEXT("ShooterShop", "Pending", "PENDING"));
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
		Text_State->SetText(FText::FromString(TEXT("Unavailable")));
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

void UShooterShopItemRow::HandleRefundClicked()
{
	if (ItemId.IsValid())
	{
		OnRefundRequested.Broadcast(ItemId);
	}
}
