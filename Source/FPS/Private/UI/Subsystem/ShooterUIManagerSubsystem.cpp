// ShooterUIManagerSubsystem.cpp
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

void UShooterUIManagerSubsystem::InitializeUI(
	APlayerController* OwningController,
	TSubclassOf<UShooterPrimaryGameLayout> RootLayoutClass)
{
	if (!OwningController)
	{
		UE_LOG(LogTemp, Warning, TEXT("!OwningController Subsystem Is not Valid"));
		return;
	}
	
	// 이미 이 컨트롤러로 RootLayout이 객체에 올라와있으면 아무것도 안 한다
	if (IsValid(RootLayoutInstance) && RootLayoutInstance->IsInViewport() )
	{
		UE_LOG(LogTemp, Warning, TEXT("IsValid(RootLayoutInstance) && RootLayoutInstance->GetOwningPlayer() == OwningController Subsystem Is not Valid"));
		return;
	}
	
	// 맵을 옮기면 PlayerController 는 새로 생기지만 RootLayout 의 Outer 가 GameInstance 라 위젯 객체는 살아남는다. 
	// 뷰포트에선 제거되고 그대로 두면 살아있는데 화면에 없는 상태가 되므로 버리고 다시 만든다
	if (IsValid(RootLayoutInstance))
	{
		UE_LOG(LogTemp, Warning, TEXT("RootLayoutInstance is Valid, RemoveFromParent."));
		RootLayoutInstance->RemoveFromParent();
		RootLayoutInstance = nullptr;
	}
	
	RootLayoutInstance = CreateWidget<UShooterPrimaryGameLayout>(OwningController, RootLayoutClass);
	if (!RootLayoutInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("RootLayoutInstance is not Valid"));
		return;
	}
	
	RootLayoutInstance->AddToViewport(-1);
	UE_LOG(LogTemp, Warning, TEXT("Subsystem Is Valid"));
}

UCommonActivatableWidget* UShooterUIManagerSubsystem::PushWidgetToLayer(
	FGameplayTag LayerTag,
	TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	// RootLayout 이 없는데 Push 를 부른 경로가 있다면 다음 의심
	// InitializeUI 전에 불림, 생성이 실패, 서버에서 UI 코드가 돎
	if (!ensure(RootLayoutInstance))
	{
		return nullptr;
	}

	return RootLayoutInstance->PushWidgetToLayer(LayerTag, WidgetClass);
}

void UShooterUIManagerSubsystem::PopWidgetFromLayer(FGameplayTag LayerTag){
	if (IsValid(RootLayoutInstance) == false)
	{
		return;
	}
	
	RootLayoutInstance->PopWidgetFromLayer(LayerTag);
}

void UShooterUIManagerSubsystem::ClearLayer(FGameplayTag LayerTag){
	if (IsValid(RootLayoutInstance) == false)
	{
		return;
	}
	
	RootLayoutInstance->ClearLayer(LayerTag);
}
