// ShooterChatInputBox.cpp
#include "UI/InGame/HUD/ShooterChatInputBox.h"

#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerController.h"

void UShooterChatInputBox::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::Collapsed);

	if (AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>())
	{
		PlayerController->RegisterChatInputWidget(this);
	}
}

FReply UShooterChatInputBox::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>();
	if (PlayerController && Text_Message)
	{
		if (InKeyEvent.GetKey() == EKeys::Enter)
		{
			PlayerController->HandleChatSubmit(Text_Message->GetText().ToString());
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::Tab)
		{
			PlayerController->HandleChatChannelSwitchRequested();
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			PlayerController->HandleChatCancel();
			return FReply::Handled();
		}
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UShooterChatInputBox::OpenForChannel(const EChatChannel Channel)
{
	SetVisibility(ESlateVisibility::Visible);

	if (Text_Message)
	{
		Text_Message->SetText(FText::GetEmpty());
	}

	SetChannel(Channel);

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputModeData;
		InputModeData.SetWidgetToFocus(TakeWidget());
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputModeData.SetHideCursorDuringCapture(true);
		PlayerController->SetInputMode(InputModeData);
	}

	if (Text_Message)
	{
		Text_Message->SetKeyboardFocus();
	}
}

void UShooterChatInputBox::SetChannel(const EChatChannel Channel)
{
	if (!Text_ChannelLabel)
	{
		return;
	}

	const FLinearColor ChannelColor = Channel == EChatChannel::Team ? TeamChannelColor : MatchChannelColor;
	Text_ChannelLabel->SetText(Channel == EChatChannel::Team ? FText::FromString(TEXT("TEAM")) : FText::FromString(TEXT("ALL")));
	Text_ChannelLabel->SetColorAndOpacity(FSlateColor(ChannelColor));
}

void UShooterChatInputBox::CloseAndClearText()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (Text_Message)
	{
		Text_Message->SetText(FText::GetEmpty());
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}
