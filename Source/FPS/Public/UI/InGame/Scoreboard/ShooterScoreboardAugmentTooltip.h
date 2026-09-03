// ShooterScoreboardAugmentTooltip.h
#pragma once

#include "CoreMinimal.h"
#include "UI/InGame/Scoreboard/ShooterAugmentChip.h"
#include "Blueprint/UserWidget.h"
#include "ShooterScoreboardAugmentTooltip.generated.h"

class UCommonTextBlock;
class UImage;

UCLASS(Abstract)
class FPS_API UShooterScoreboardAugmentTooltip : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetData(const FShooterAugmentDisplay& InData);
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Name;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Tier;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Description;
};
