#include "UI/Lobby/LobbyPlayerEntryWidget.h"

#include "Components/TextBlock.h"

void ULobbyPlayerEntryWidget::SetupLobbyPlayerEntry(
	const FString& InPlayerName,
	const bool bInIsHost,
	const bool bInIsReady)
{
	PlayerName = InPlayerName;
	bIsHost = bInIsHost;
	bIsReady = bInIsReady;

	UpdateTextBlocks();
	OnLobbyPlayerInfoUpdated(PlayerName, bIsHost, bIsReady);
}

void ULobbyPlayerEntryWidget::UpdateTextBlocks()
{
	if (PlayerNameText)
	{
		PlayerNameText->SetText(FText::FromString(PlayerName));
	}

	if (HostText)
	{
		HostText->SetText(bIsHost ? FText::FromString(TEXT("Host")) : FText::GetEmpty());
	}

	if (ReadyText)
	{
		ReadyText->SetText(bIsHost || bIsReady
			? FText::FromString(TEXT("Ready"))
			: FText::FromString(TEXT("Waiting")));
	}
}
