// ShooterLobbyScreen.cpp
#include "UI/OutGame/ShooterLobbyScreen.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Kismet/GameplayStatics.h"
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

	
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, TempDestinationMap);
}
