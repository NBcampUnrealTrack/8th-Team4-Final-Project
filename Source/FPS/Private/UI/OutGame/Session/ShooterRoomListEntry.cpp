// ShooterRoomListEntry.cpp
#include "UI/OutGame/Session/ShooterRoomListEntry.h"

#include "Components/TextBlock.h"

void UShooterRoomListEntry::SetRoomInfo(const FRoomInfo& InRoomInfo)
{
	if (IsValid(Text_RoomName))
	{
		Text_RoomName->SetText(FText::FromString(InRoomInfo.RoomName));
	}

	if (IsValid(Text_MapName))
	{
		Text_MapName->SetText(FText::FromString(InRoomInfo.MapName));
	}

	if (IsValid(Text_MatchType))
	{
		Text_MatchType->SetText(FText::FromString(InRoomInfo.MatchType));
	}

	if (IsValid(Text_PlayerCount))
	{
		Text_PlayerCount->SetText(FText::Format(
			NSLOCTEXT("ShooterLobby", "RoomPlayerCount", "{0} / {1}"),
			FText::AsNumber(InRoomInfo.CurrentPlayers),
			FText::AsNumber(InRoomInfo.MaxPlayers)));
	}

	if (IsValid(Text_Ping))
	{
		Text_Ping->SetText(FText::Format(
			NSLOCTEXT("ShooterLobby", "RoomPing", "{0} ms"),
			FText::AsNumber(InRoomInfo.PingInMs)));
	}

	if (IsValid(Text_JoinState))
	{
		Text_JoinState->SetText(NSLOCTEXT("ShooterLobby", "RoomInProgress", "진행 중"));
		Text_JoinState->SetVisibility(InRoomInfo.bIsJoinable
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}

	SetIsEnabled(InRoomInfo.bIsJoinable);
}
