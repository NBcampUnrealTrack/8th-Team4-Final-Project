// ShotterPrimaryGameLayout.cpp
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "Tags/UITags.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "UI/Foundation/ShooterActivatableWidget.h"

void UShooterPrimaryGameLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RegisterLayer(UITags::UI_Layer_Game,  Stack_Game);
	RegisterLayer(UITags::UI_Layer_Menu,  Stack_Menu);
	RegisterLayer(UITags::UI_Layer_Modal, Stack_Modal);
	
}

void UShooterPrimaryGameLayout::RegisterLayer(
	FGameplayTag LayerTag, 
	UCommonActivatableWidgetContainerBase* LayerWidget)
{
	if (IsValid(LayerWidget) == false)
	{
		return;
	}	
	
	LayerContainers.Add(LayerTag, LayerWidget);
}

UCommonActivatableWidget* UShooterPrimaryGameLayout::PushWidgetToLayer(
	FGameplayTag LayerTag,
	TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (IsValid(WidgetClass) == false)
	{
		return nullptr;
	}
	
	if (UCommonActivatableWidgetContainerBase* Container = GetLayerContainer(LayerTag))
	{
		return Container->AddWidget<UCommonActivatableWidget>(WidgetClass);
	}


	return nullptr;
}

void UShooterPrimaryGameLayout::PopWidgetFromLayer(FGameplayTag LayerTag)
{
	if (UCommonActivatableWidgetContainerBase* Container = GetLayerContainer(LayerTag))
	{
		// 레이어가 비어 있어도 정상, 닫기 버튼 연타, 전환 애니메이션 진행 중 이 있을 수 있음
		if (UCommonActivatableWidget* ActiveWidget = Container->GetActiveWidget())
		{
			Container->RemoveWidget(*ActiveWidget);
		}
	}
}

void UShooterPrimaryGameLayout::ClearLayer(FGameplayTag LayerTag)
{
	if (UCommonActivatableWidgetContainerBase* Container = GetLayerContainer(LayerTag))
	{
		Container->ClearWidgets();
	}
}

UCommonActivatableWidgetContainerBase* UShooterPrimaryGameLayout::GetLayerContainer(FGameplayTag LayerTag) const
{
	UCommonActivatableWidgetContainerBase* Container = LayerContainers.FindRef(LayerTag);
	ensureMsgf(Container, TEXT("등록되지 않은 레이어 태그: %s"), *LayerTag.ToString());
	return Container;
}


