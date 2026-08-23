#include "Shop/RogueShopSettings.h"

#include "Shop/ShopCatalogDataAsset.h"

URogueShopSettings::URogueShopSettings()
{
	// AugmentSelecting을 상점 구매와 증강 선택이 함께 열리는 준비 구간으로 사용한다.
	AllowedPurchasePhases =
	{
		EGameFlowPhase::AugmentSelecting
	};
}

const UShopCatalogDataAsset* URogueShopSettings::LoadCatalog() const
{
	if (const UShopCatalogDataAsset* LoadedCatalog = Catalog.Get())
	{
		return LoadedCatalog;
	}

	return Catalog.IsNull() ? nullptr : Catalog.LoadSynchronous();
}

bool URogueShopSettings::IsPurchasePhaseAllowed(const EGameFlowPhase Phase) const
{
	return AllowedPurchasePhases.Contains(Phase);
}

bool URogueShopSettings::IsConfigurationValid(FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const FString& FailureReason)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FailureReason;
			}
			return false;
		};

	const UShopCatalogDataAsset* LoadedCatalog = LoadCatalog();
	if (!IsValid(LoadedCatalog))
	{
		return Fail(TEXT("Shop Catalog DataAsset is not configured or could not be loaded."));
	}

	FString CatalogFailureReason;
	if (!LoadedCatalog->IsConfigurationValid(&CatalogFailureReason))
	{
		return Fail(FString::Printf(
			TEXT("Shop catalog is invalid: %s"),
			CatalogFailureReason.IsEmpty() ? TEXT("Unknown catalog error") : *CatalogFailureReason));
	}

	if (AllowedPurchasePhases.IsEmpty())
	{
		return Fail(TEXT("No purchase phase is configured."));
	}

	const UEnum* GameFlowPhaseEnum = StaticEnum<EGameFlowPhase>();
	if (!GameFlowPhaseEnum)
	{
		return Fail(TEXT("The game flow phase enum is unavailable."));
	}

	TSet<EGameFlowPhase> UniquePhases;
	for (const EGameFlowPhase Phase : AllowedPurchasePhases)
	{
		if (!GameFlowPhaseEnum->IsValidEnumValue(static_cast<int64>(Phase)))
		{
			return Fail(TEXT("An invalid game flow phase is configured for shop purchases."));
		}

		if (UniquePhases.Contains(Phase))
		{
			return Fail(FString::Printf(
				TEXT("Purchase phase %s is configured more than once."),
				*UEnum::GetValueAsString(Phase)));
		}

		UniquePhases.Add(Phase);
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	return true;
}
