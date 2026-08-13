// ShooterActivatableWidget.h
#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "ShooterActivatableWidget.generated.h"

// 화면이 활성화된 동안 적용할 InputMode
UENUM(BlueprintType)
enum class EShooterWidgetInputMode : uint8
{
	Default,    
	// UI 조작 중에도 이동 가능. BuyPhase, 증강 선택 
	GameAndMenu,
	// 게임 입력만. 커서 없음. HUD 
	Game,
	// UI 입력만. 커서 표시. 로비, 설정, 일시정지 
	Menu
};
 
UCLASS(Abstract, Blueprintable)
class FPS_API UShooterActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
public:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	
protected:
	// WBP마다 Details에서 지정하는 InputMode
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EShooterWidgetInputMode InputConfig = EShooterWidgetInputMode::Default;
	// 초기세팅은 커서가 보이지않음. GameOnly
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;
};
