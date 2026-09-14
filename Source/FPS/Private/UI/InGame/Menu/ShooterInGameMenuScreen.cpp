#include "UI/InGame/Menu/ShooterInGameMenuScreen.h"

#include "CommonButtonBase.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

UShooterInGameMenuScreen::UShooterInGameMenuScreen()
{
	InputConfig = EShooterWidgetInputMode::Menu;
}

void UShooterInGameMenuScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 기존 WBP_PrimaryButton(CommonUI)와 자동 생성 WBP의 UButton을 둘 다 지원한다.
	if (UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(Btn_Resume))
	{
		CommonButton->OnClicked().AddUObject(this, &ThisClass::HandleResumeClicked);
	}
	else if (UButton* UMGButton = Cast<UButton>(Btn_Resume))
	{
		UMGButton->OnClicked.AddDynamic(this, &ThisClass::HandleResumeClicked);
	}

	if (IsValid(Btn_Options))
	{
		if (UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(Btn_Options))
		{
			CommonButton->OnClicked().AddUObject(this, &ThisClass::HandleOptionsClicked);
		}
		else if (UButton* UMGButton = Cast<UButton>(Btn_Options))
		{
			UMGButton->OnClicked.AddDynamic(this, &ThisClass::HandleOptionsClicked);
		}
	}

	if (UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(Btn_Surrender))
	{
		CommonButton->OnClicked().AddUObject(this, &ThisClass::HandleSurrenderClicked);
	}
	else if (UButton* UMGButton = Cast<UButton>(Btn_Surrender))
	{
		UMGButton->OnClicked.AddDynamic(this, &ThisClass::HandleSurrenderClicked);
	}

	if (UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(Btn_ReturnToLobby))
	{
		CommonButton->OnClicked().AddUObject(this, &ThisClass::HandleReturnToLobbyClicked);
	}
	else if (UButton* UMGButton = Cast<UButton>(Btn_ReturnToLobby))
	{
		UMGButton->OnClicked.AddDynamic(this, &ThisClass::HandleReturnToLobbyClicked);
	}

	if (UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(Btn_QuitGame))
	{
		CommonButton->OnClicked().AddUObject(this, &ThisClass::HandleQuitGameClicked);
	}
	else if (UButton* UMGButton = Cast<UButton>(Btn_QuitGame))
	{
		UMGButton->OnClicked.AddDynamic(this, &ThisClass::HandleQuitGameClicked);
	}
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

void UShooterInGameMenuScreen::HandleOptionsClicked()
{
	OnOptionsRequested.Broadcast();
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
