#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameMode/Types/RoundTypes.h"

#include "RogueShopSettings.generated.h"

class UShopCatalogDataAsset;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Shop System"))
class FPS_API URogueShopSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URogueShopSettings();

	const UShopCatalogDataAsset* LoadCatalog() const;
	bool IsPurchasePhaseAllowed(EGameFlowPhase Phase) const;
	bool IsConfigurationValid(FString* OutFailureReason = nullptr) const;

	// 실제 상품 정의는 Content Browser의 ShopCatalog DataAsset 한 곳에서 관리한다.
	UPROPERTY(Config, EditAnywhere, Category = "Catalog")
	TSoftObjectPtr<UShopCatalogDataAsset> Catalog;

	// 현재 게임 흐름에서 구매 요청을 허용할 페이즈다. 서버가 이 값을 기준으로 최종 검증한다.
	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	TArray<EGameFlowPhase> AllowedPurchasePhases;
};
