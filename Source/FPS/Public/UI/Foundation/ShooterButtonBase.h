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
	// 런타임 텍스트 변경시 사용하기 위함
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetButtonText(const FText& InText);
	
protected:
	// 수정할때 보이도록 PreConstruct사용
	virtual void NativePreConstruct() override;
	
	// WBP 그래프에서 구현 - 넘어온 텍스트를 자기 Text위젯에 연결
	// BindWidget으로 두지않아야지 아이콘만 있는 버튼과 같은 예외에서도 쓸 수 있어 재사용성 확보
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void UpdateButtonText(const FText& InText);
	
	UPROPERTY(EditAnywhere, Category = "UI")
	FText ButtonText;
};
