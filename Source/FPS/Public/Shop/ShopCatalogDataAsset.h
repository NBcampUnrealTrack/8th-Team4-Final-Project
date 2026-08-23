#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Shop/ShopTypes.h"

#include "ShopCatalogDataAsset.generated.h"

class UTexture2D;
class UShooterShopItemRow;

/** 디자이너가 편집하는 상품별 UI 표시 데이터다. */
USTRUCT(BlueprintType)
struct FShopItemPresentation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UTexture2D> PreviewImage;

	// 비어 있으면 ShopScreen의 기본 Row 클래스를 사용한다.
	// 지정한 경우 UShooterShopItemRow 파생 위젯이어야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TSoftClassPtr<UShooterShopItemRow> WidgetClassOverride;
};

/** 하나의 상품에 대한 거래 데이터와 UI 표시 데이터를 함께 보관한다. */
USTRUCT(BlueprintType)
struct FShopCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FShopItemDefinition Definition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FShopItemPresentation Presentation;
};

/**
 * 상점에서 판매하는 상품의 단일 데이터 원본이다.
 * Project Settings는 이 Asset을 가리키고, 거래 코어와 UI가 같은 Catalog를 읽는다.
 */
UCLASS(BlueprintType)
class FPS_API UShopCatalogDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	const FShopCatalogEntry* FindItem(FGameplayTag ItemId) const;
	const FShopItemDefinition* FindItemDefinition(FGameplayTag ItemId) const;
	bool IsConfigurationValid(FString* OutFailureReason = nullptr) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	TArray<FShopCatalogEntry> Items;
};
