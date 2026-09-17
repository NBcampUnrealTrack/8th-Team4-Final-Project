// ShooterButtonBase.h
#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "ShooterButtonBase.generated.h"

/**
 * 프로젝트 모든 버튼의 부모로 텍스트를 C++ 이 들고, 어디에 그릴지는 WBP에서 정함
 */
UCLASS(Abstract, Blueprintable)
class FPS_API UShooterButtonBase : public UCommonButtonBase{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetButtonText(const FText& InText);
	
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnCurrentTextStyleChanged() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void UpdateButtonText(const FText& InText);
	
	UPROPERTY(EditAnywhere, Category = "UI")
	FText ButtonText;

private:
	void ApplyCurrentTextColor();
};
