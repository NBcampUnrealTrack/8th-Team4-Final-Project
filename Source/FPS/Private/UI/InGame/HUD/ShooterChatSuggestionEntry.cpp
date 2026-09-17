// ShooterChatSuggestionEntry.cpp
#include "UI/InGame/HUD/ShooterChatSuggestionEntry.h"

#include "CommonTextBlock.h"

void UShooterChatSuggestionEntry::SetPlayerName(const FString& InPlayerName)
{
	if (Text_Name)
	{
		Text_Name->SetText(FText::FromString(InPlayerName));
	}
}

void UShooterChatSuggestionEntry::SetHighlighted(const bool bInHighlighted)
{
	if (Text_Name)
	{
		Text_Name->SetColorAndOpacity(FSlateColor(bInHighlighted ? HighlightedColor : NormalColor));
	}
}
