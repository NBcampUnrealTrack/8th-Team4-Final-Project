// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RoomEntryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/MenuWidget.h"

void URoomEntryWidget::SetupRoomEntry(const FRoomInfo& InRoomInfo, UMenuWidget* InOwningMenu)
{
	RoomInfo = InRoomInfo;
	OwningMenu = InOwningMenu;

	UpdateTextBlocks();
	OnRoomInfoUpdated(RoomInfo);
}

bool URoomEntryWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}

	return true;
}

void URoomEntryWidget::JoinButtonClicked()
{
	if (!OwningMenu)
	{
		return;
	}

	OwningMenu->JoinRoom(RoomInfo.SessionIndex);
}

void URoomEntryWidget::UpdateTextBlocks()
{
	if (RoomNameText)
	{
		RoomNameText->SetText(FText::FromString(RoomInfo.RoomName));
	}

	if (MapNameText)
	{
		MapNameText->SetText(FText::FromString(RoomInfo.MapName));
	}

	if (MatchTypeText)
	{
		MatchTypeText->SetText(FText::FromString(RoomInfo.MatchType));
	}

	if (PlayerCountText)
	{
		PlayerCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), RoomInfo.CurrentPlayers, RoomInfo.MaxPlayers)));
	}

	if (PingText)
	{
		PingText->SetText(FText::FromString(FString::Printf(TEXT("%d ms"), RoomInfo.PingInMs)));
	}
}
