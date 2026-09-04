#include "UI/InGame/Menu/ShooterInGameMenuScreen.h"

#include "CommonButtonBase.h"
#include "Components/TextBlock.h"

UShooterInGameMenuScreen::UShooterInGameMenuScreen()
{
	InputConfig = EShooterWidgetInputMode::Menu;
}

void UShooterInGameMenuScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Btn_Resume->OnClicked().AddUObject(this, &ThisClass::HandleResumeClicked);
	Btn_ReturnToLobby->OnClicked().AddUObject(this, &ThisClass::HandleReturnToLobbyClicked);
	Btn_QuitGame->OnClicked().AddUObject(this, &ThisClass::HandleQuitGameClicked);
	Btn_Surrender->OnClicked().AddUObject(this, &ThisClass::HandleSurrenderClicked);
}

void UShooterInGameMenuScreen::SetSurrenderEnabled(const bool bEnabled)
{
	Btn_Surrender->SetIsEnabled(bEnabled);
}

void UShooterInGameMenuScreen::SetStatusText(const FText& StatusText)
{
	Text_Status->SetText(StatusText);
}

void UShooterInGameMenuScreen::HandleResumeClicked()
{
	OnResumeRequested.Broadcast();
}

void UShooterInGameMenuScreen::HandleReturnToLobbyClicked()
{
	OnReturnToLobbyRequested.Broadcast();
}

void UShooterInGameMenuScreen::HandleQuitGameClicked()
{
	OnQuitGameRequested.Broadcast();
}

void UShooterInGameMenuScreen::HandleSurrenderClicked()
{
	OnSurrenderRequested.Broadcast();
}

bool UShooterInGameMenuScreen::NativeOnHandleBackAction()
{
	OnResumeRequested.Broadcast();
	return true;
}
