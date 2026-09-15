// ShooterChatEntry.cpp
#include "UI/InGame/HUD/ShooterChatEntry.h"

#include "CommonTextBlock.h"

void UShooterChatEntry::SetData(const FChatMessage& InMessage)
{
	const FLinearColor ChannelColor =
		InMessage.Channel == EChatChannel::Team ? TeamChannelColor : MatchChannelColor;

	if (Text_SenderName)
	{
		Text_SenderName->SetText(FText::FromString(InMessage.SenderName));
		Text_SenderName->SetColorAndOpacity(FSlateColor(ChannelColor));
	}

	if (Text_Message)
	{
		Text_Message->SetText(FText::FromString(InMessage.MessageText));
	}

	OnChannelApplied(InMessage.Channel);
}
