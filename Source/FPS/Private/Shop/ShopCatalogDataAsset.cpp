#include "Shop/ShopCatalogDataAsset.h"

const FShopCatalogEntry* UShopCatalogDataAsset::FindItem(const FGameplayTag ItemId) const
{
	if (!ItemId.IsValid())
	{
		return nullptr;
	}

	return Items.FindByPredicate(
		[ItemId](const FShopCatalogEntry& Entry)
		{
			return Entry.Definition.ItemId.MatchesTagExact(ItemId);
		});
}

const FShopItemDefinition* UShopCatalogDataAsset::FindItemDefinition(const FGameplayTag ItemId) const
{
	const FShopCatalogEntry* Entry = FindItem(ItemId);
	return Entry ? &Entry->Definition : nullptr;
}

bool UShopCatalogDataAsset::IsConfigurationValid(FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const FString& FailureReason)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FailureReason;
			}
			return false;
		};

	if (Items.IsEmpty())
	{
		return Fail(TEXT("The shop catalog is empty."));
	}

	const UEnum* ItemTypeEnum = StaticEnum<EShopItemType>();
	if (!ItemTypeEnum)
	{
		return Fail(TEXT("The shop item type enum is unavailable."));
	}

	TSet<FGameplayTag> UniqueItemIds;
	for (const FShopCatalogEntry& Entry : Items)
	{
		const FShopItemDefinition& Definition = Entry.Definition;

		if (!Definition.ItemId.IsValid())
		{
			return Fail(TEXT("A shop item has an invalid ItemId."));
		}

		if (!ItemTypeEnum->IsValidEnumValue(static_cast<int64>(Definition.ItemType))
			|| Definition.ItemType == EShopItemType::None)
		{
			return Fail(FString::Printf(
				TEXT("Shop item %s has an invalid item type."),
				*Definition.ItemId.ToString()));
		}

		if (Definition.BasePrice <= 0)
		{
			return Fail(FString::Printf(
				TEXT("Shop item %s has a non-positive base price."),
				*Definition.ItemId.ToString()));
		}

		for (const int32 TierPrice : Definition.TierPrices)
		{
			if (TierPrice <= 0)
			{
				return Fail(FString::Printf(
					TEXT("Shop item %s has a non-positive tier price."),
					*Definition.ItemId.ToString()));
			}
		}

		if (UniqueItemIds.Contains(Definition.ItemId))
		{
			return Fail(FString::Printf(
				TEXT("Shop item %s is duplicated in the catalog."),
				*Definition.ItemId.ToString()));
		}

		UniqueItemIds.Add(Definition.ItemId);
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	return true;
}
