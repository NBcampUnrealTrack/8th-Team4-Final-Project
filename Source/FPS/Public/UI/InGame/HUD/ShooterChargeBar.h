// ShooterChargeBar.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ShooterChargeBar.generated.h"

class UWidget;

/**
 * SkillSlot의 충전 한 칸
 * 빈 칸(외곽선)은 WBP에 항상 깔아두고, 채워진 이미지만 켜고 끈다(이미지 교체 방식)
 */
UCLASS()
class FPS_API UShooterChargeBar : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Skill Slot")
	void SetFilled(bool bFilled);
	
protected:
	// Fill 상태 이미지
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Image_Fill;
};
