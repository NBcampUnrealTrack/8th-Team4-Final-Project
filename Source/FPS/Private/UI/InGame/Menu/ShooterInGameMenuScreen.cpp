#include "UI/InGame/Menu/ShooterInGameMenuScreen.h"

#include "Animation/WidgetAnimation.h"
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

	if (Anim_Exit)
	{
		ExitFinishedDelegate.BindDynamic(this, &ThisClass::HandleExitFinished);
		BindToAnimationFinished(Anim_Exit, ExitFinishedDelegate);
	}
}

void UShooterInGameMenuScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 스택이 인스턴스를 재사용하고, 옵션에서 돌아올 때도 다시 불린다.
	bClosing = false;
	if (Anim_Exit)
	{
		StopAnimation(Anim_Exit);
	}
	if (Anim_Enter)
	{
		PlayAnimation(Anim_Enter);
	}
}

void UShooterInGameMenuScreen::RequestResume()
{
	if (bClosing)
	{
		return;
	}

	if (!Anim_Exit)
	{
		OnResumeRequested.Broadcast();
		return;
	}

	bClosing = true;
	PlayAnimation(Anim_Exit);
}

void UShooterInGameMenuScreen::HandleExitFinished()
{
	if (!bClosing)
	{
		return;
	}

	bClosing = false;
	OnResumeRequested.Broadcast();
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
	RequestResume();
}

void UShooterInGameMenuScreen::HandleOptionsClicked()
{
	if (bClosing)
	{
		return;
	}
	OnOptionsRequested.Broadcast();
}

void UShooterInGameMenuScreen::HandleReturnToLobbyClicked()
{
	if (bClosing)
	{
		return;
	}
	OnReturnToLobbyRequested.Broadcast();
}

void UShooterInGameMenuScreen::HandleQuitGameClicked()
{
	if (bClosing)
	{
		return;
	}
	OnQuitGameRequested.Broadcast();
}

void UShooterInGameMenuScreen::HandleSurrenderClicked()
{
	if (bClosing)
	{
		return;
	}
	OnSurrenderRequested.Broadcast();
}

bool UShooterInGameMenuScreen::NativeOnHandleBackAction()
{
	RequestResume();
	return true;
}
