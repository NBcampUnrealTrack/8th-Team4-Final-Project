// ShooterUIInitComponent.cpp
#include "UI/Frontend/ShooterUIInitComponent.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

void UShooterUIInitComponent::BeginPlay(){
	Super::BeginPlay();
	
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!ensure(PC))
	{
		return;
	}
	
	if (!PC->IsLocalController())
	{
		return;
	}
	
	UGameInstance* GameInstance = PC->GetGameInstance();
	if (IsValid(GameInstance) == false)
	{
		return;
	}
	
	UShooterUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UShooterUIManagerSubsystem>();
	if (IsValid(UIManager) == false)
	{
		return;
	}
	
	UIManager->InitializeUI(PC, RootLayoutClass);
	
	// BP에서 지정
	if (!ensure(InitialWidgetClass))
	{
		return;
	}

	UIManager->PushWidgetToLayer(InitialWidgetLayer, InitialWidgetClass);
}
