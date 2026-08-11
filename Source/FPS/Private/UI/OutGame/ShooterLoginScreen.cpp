// ShooterLoginScreen.cpp
#include "UI/OutGame/ShooterLoginScreen.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "Tags/UITags.h"
#include "CommonButtonBase.h"
#include "Engine/GameInstance.h"

void UShooterLoginScreen::NativeOnInitialized(){
	Super::NativeOnInitialized();
	
	Btn_EnterLobby->OnClicked().AddUObject(this, &ThisClass::HandleEnterLobbyClicked);
}

void UShooterLoginScreen::HandleEnterLobbyClicked(){
	const TSubclassOf<UCommonActivatableWidget> LoadedClass = LobbyWidgetClass.LoadSynchronous();
	
	if (IsValid(LoadedClass) == false)
	{
		return;
	}
	
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UShooterUIManagerSubsystem>();

	if (!UIManager)
	{
		return;
	}
	
	// Menu에 쌓지 않고 교체한다, 로비에 들어간 뒤 Back을 눌러 로그인으로 돌아갈 일이 없음.
	// Clear하여 Lobby가 Menu의 루트로 만듦
	UIManager->ClearLayer(UITags::UI_Layer_Menu);
	UIManager->PushWidgetToLayer(UITags::UI_Layer_Menu, LoadedClass);
}
