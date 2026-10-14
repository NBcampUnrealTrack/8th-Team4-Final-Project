// ShooterAugmentChip.h
#pragma once

#include "CoreMinimal.h"
#include "Augment/AugmentTypes.h"
#include "Blueprint/UserWidget.h"
#include "ShooterAugmentChip.generated.h"

class UCommonTextBlock;
class UImage;
class UShooterAugmentChip;

/**
 * 스코어보드 증강 한 칸에 필요한 값 
 */
USTRUCT(BlueprintType)
struct FShooterAugmentDisplay
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	EAugmentTier Tier = EAugmentTier::Common;
	// 스코어보드가 DA_AugmentTierStyleSet에서 미리 풀어서 채운다.
	UPROPERTY(BlueprintReadOnly)
	FText TierDisplayName;
	UPROPERTY(BlueprintReadOnly)
	FLinearColor TierAccentColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;
	UPROPERTY(BlueprintReadOnly)
	FText Description;
	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;
	// 액티브 슬롯 번호, 패시브는 INDEX_NONE
	UPROPERTY(BlueprintReadOnly)
	int32 ActiveSlotIndex = INDEX_NONE;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAugmentChipHovered, 
	const FShooterAugmentDisplay&, UShooterAugmentChip*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAugmentChipUnHovered, UShooterAugmentChip*);

UCLASS(Abstract, meta = (DisableNativeTick))
class FPS_API UShooterAugmentChip : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetData(const FShooterAugmentDisplay& InData);
	void SetEmpty();
	
	FOnAugmentChipHovered OnChipHovered;
	FOnAugmentChipUnHovered OnChipUnHovered;
	
protected:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	
	// 행 칩은 아이콘만 쓴다. 이름/등급/설명은 툴팁 담당이라 Optional
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_AugmentName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;
	
private:
	// Hover시 위로 넘김 툴팁이 재조회 하지않게함
	FShooterAugmentDisplay CachedData;
	// 빈칸은 호버해도 띄우지 않음
	bool bHasData = false;
};
