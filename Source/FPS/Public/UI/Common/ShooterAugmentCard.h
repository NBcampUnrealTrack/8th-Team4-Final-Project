// ShooterAugmentCard.h
#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "ShooterAugmentCard.generated.h"

class UBorder;
class UCommonTextBlock;
class UImage;

/**
 * 증강 카드 한 장 레이어에 올리지 않으므로 UUserWidget
 */
UCLASS(Abstract)
class FPS_API UShooterAugmentCard : public UCommonButtonBase{
	GENERATED_BODY()
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Border_Tier;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Name;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Description;
};
