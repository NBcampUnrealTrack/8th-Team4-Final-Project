// ShooterAugmentSelectScreen.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "UI/Common/ShooterAugmentCard.h"
#include "ShooterAugmentSelectScreen.generated.h"

class UHorizontalBox;

/**
 * 증강 선택 화면으로 Modal 레이어에 올린다.
 */
UCLASS(Abstract)
class FPS_API UShooterAugmentSelectScreen : public UShooterActivatableWidget{
	GENERATED_BODY()
	
protected:
	// ShooterAugmentCard가 채워지는 자리
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> Box_Cards;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UShooterAugmentCard> CardClass;
};
