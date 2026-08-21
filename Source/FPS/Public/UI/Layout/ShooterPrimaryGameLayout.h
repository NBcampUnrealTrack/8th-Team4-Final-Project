// ShooterPrimaryGameLayout.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "ShooterPrimaryGameLayout.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;
class UCommonActivatableWidgetContainerBase;

/**
 * 화면에 쌓이는 레이어(Game / Menu / Modal)를 담는 컨테이너 클래스
 * RegisterLayer로 Tag에 등록하고 컨테이너에 등록한다.
 * PushWidgetToLayer로 태그로 Stack을 찾아 화면을 Stack하고 PopWidgetFromLayer로 pop한다.
 */
UCLASS(Abstract)
class FPS_API UShooterPrimaryGameLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UCommonActivatableWidget* PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);
	
	void PopWidgetFromLayer(FGameplayTag LayerTag);
	
	// 레이어 태그 전체 비우기. HUD 화면 정리 등에 필요
	void ClearLayer(FGameplayTag LayerTag);
	
protected:
	virtual void NativeOnInitialized() override;
	
	void RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerWidget);
	
	UCommonActivatableWidgetContainerBase* GetLayerContainer(FGameplayTag LayerTag) const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Stack_Game;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Stack_Menu;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Stack_Modal;
	
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>> LayerContainers;
};
