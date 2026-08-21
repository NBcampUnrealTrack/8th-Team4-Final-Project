#include "Shop/RogueShopSettings.h"
#include "Shop/ShopTags.h"

URogueShopSettings::URogueShopSettings()
{
	// AugmentSelecting을 상점 구매와 증강 선택이 함께 열리는 준비 구간으로 사용한다.
	AllowedPurchasePhases =
	{
		EGameFlowPhase::AugmentSelecting
	};

	// 서버가 신뢰하는 기본 상품 카탈로그다. 클라이언트가 전달한 가격은 구매 판정에 사용하지 않는다.
	ItemDefinitions =
	{
		FShopItemDefinition(
			RogueShopTags::Item_Weapon_Rifle,
			EShopItemType::Weapon,
			1500,
			NSLOCTEXT("RogueShop", "Rifle", "Rifle")),
		FShopItemDefinition(
			RogueShopTags::Item_Weapon_Sniper,
			EShopItemType::Weapon,
			1500,
			NSLOCTEXT("RogueShop", "SniperRifle", "Sniper Rifle")),
		FShopItemDefinition(
			RogueShopTags::Item_Weapon_Shotgun,
			EShopItemType::Weapon,
			1500,
			NSLOCTEXT("RogueShop", "Shotgun", "Shotgun")),
		FShopItemDefinition(
			RogueShopTags::Item_WeaponUpgrade_Pistol,
			EShopItemType::WeaponUpgrade,
			1000,
			NSLOCTEXT("RogueShop", "PistolUpgrade", "Pistol Upgrade")),
		FShopItemDefinition(
			RogueShopTags::Item_WeaponUpgrade_Knife,
			EShopItemType::WeaponUpgrade,
			1000,
			NSLOCTEXT("RogueShop", "KnifeUpgrade", "Knife Upgrade")),
		FShopItemDefinition(
			RogueShopTags::Item_HealthUpgrade,
			EShopItemType::HealthUpgrade,
			200,
			NSLOCTEXT("RogueShop", "HealthUpgrade", "Health Upgrade")),
		FShopItemDefinition(
			RogueShopTags::Item_Shield_Small,
			EShopItemType::Shield,
			150,
			NSLOCTEXT("RogueShop", "SmallShield", "Small Shield")),
		FShopItemDefinition(
			RogueShopTags::Item_Shield_Large,
			EShopItemType::Shield,
			400,
			NSLOCTEXT("RogueShop", "LargeShield", "Large Shield")),
		FShopItemDefinition(
			RogueShopTags::Item_DamageUpgrade,
			EShopItemType::DamageUpgrade,
			300,
			NSLOCTEXT("RogueShop", "DamageUpgrade", "Damage Upgrade")),
		FShopItemDefinition(
			RogueShopTags::Item_Augment,
			EShopItemType::Augment,
			1000,
			NSLOCTEXT("RogueShop", "Augment", "Augment"))
	};
}

const FShopItemDefinition* URogueShopSettings::FindItemDefinition(const FGameplayTag ItemId) const
{
	if (!ItemId.IsValid())
	{
		return nullptr;
	}

	// 부모/자식 태그까지 허용하지 않고 요청한 상품 ID와 정확히 일치하는 항목만 조회한다.
	return ItemDefinitions.FindByPredicate(
		[ItemId](const FShopItemDefinition& Definition)
		{
			return Definition.ItemId.MatchesTagExact(ItemId);
		});
}

bool URogueShopSettings::IsPurchasePhaseAllowed(const EGameFlowPhase Phase) const
{
	return AllowedPurchasePhases.Contains(Phase);
}

bool URogueShopSettings::IsConfigurationValid(FString* OutFailureReason) const
{
	// 검증 실패 처리를 한 경로로 모아 호출자에게 동일한 방식으로 원인을 전달한다.
	auto Fail = [OutFailureReason](const FString& FailureReason)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FailureReason;
			}
			return false;
		};

	if (ItemDefinitions.IsEmpty())
	{
		return Fail(TEXT("The shop catalog is empty."));
	}

	const UEnum* ItemTypeEnum = StaticEnum<EShopItemType>();
	if (!ItemTypeEnum)
	{
		return Fail(TEXT("The shop item type enum is unavailable."));
	}

	// 상품 ID 중복은 조회 결과가 설정 순서에 따라 달라질 수 있으므로 설정 오류로 처리한다.
	TSet<FGameplayTag> UniqueItemIds;
	for (const FShopItemDefinition& Definition : ItemDefinitions)
	{
		if (!Definition.ItemId.IsValid())
		{
			return Fail(TEXT("A shop item has an invalid ItemId."));
		}

		// 역직렬화되거나 잘못 설정된 Enum 값까지 방어하고 None은 실제 상품 타입으로 허용하지 않는다.
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

		if (UniqueItemIds.Contains(Definition.ItemId))
		{
			return Fail(FString::Printf(
				TEXT("Shop item %s is duplicated in the catalog."),
				*Definition.ItemId.ToString()));
		}

		UniqueItemIds.Add(Definition.ItemId);
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

	// 구매 가능 Phase도 중복이나 유효하지 않은 설정을 시작 시점에 차단한다.
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