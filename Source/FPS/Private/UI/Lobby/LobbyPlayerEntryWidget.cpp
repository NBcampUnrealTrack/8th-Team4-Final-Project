#include "UI/Lobby/LobbyPlayerEntryWidget.h"

#include "Components/TextBlock.h"

void ULobbyPlayerEntryWidget::SetupLobbyPlayerEntry(
	const FString& InPlayerName,
	const bool bInIsHost,
	const bool bInIsReady,
	const EGameTeam InTeam)
{
	PlayerName = InPlayerName;
	bIsHost = bInIsHost;
	bIsReady = bInIsReady;
	Team = InTeam;

	UpdateTextBlocks();
	OnLobbyPlayerInfoUpdated(PlayerName, bIsHost, bIsReady, Team);
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

	if (TeamText)
	{
		TeamText->SetText(GetTeamDisplayText(Team));
	}
}

FText ULobbyPlayerEntryWidget::GetTeamDisplayText(const EGameTeam InTeam)
{
	switch (InTeam)
	{
	case EGameTeam::TeamA:
		return FText::FromString(TEXT("[Red]"));
	case EGameTeam::TeamB:
		return FText::FromString(TEXT("[Blue]"));
	default:
		return FText::FromString(TEXT("[Random]"));
	}
}
