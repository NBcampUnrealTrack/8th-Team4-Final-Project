// ShooterLobbyScreen.cpp
#include "UI/OutGame/ShooterLobbyScreen.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Engine/GameInstance.h"
#include "Session/SessionSubSystem.h"
#include "CommonButtonBase.h"


void UShooterLobbyScreen::NativeOnInitialized(){
	Super::NativeOnInitialized();
	
	Btn_Play->OnClicked().AddUObject(this, &ThisClass::HandlePlayClicked);
}

void UShooterLobbyScreen::HandlePlayClicked(){
	if (TempDestinationMap.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lobby] Temp Destination Map 이 비어 있습니다"));
		return;
	}

	
	USessionSubSystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<USessionSubSystem>() : nullptr;
	if (!Sessions || !Sessions->LeaveRoomAndOpenLocalMap(TempDestinationMap))
		UE_LOG(LogTemp, Warning, TEXT("[Lobby] A room transition is already active"));
}
