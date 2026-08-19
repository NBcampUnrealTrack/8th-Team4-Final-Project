// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Session/MainMenuWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SpinBox.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"
#include "Session/SessionSubSystem.h"
#include "UI/Session/RoomEntryWidget.h"

void UMainMenuWidget::HostRoom()
{
	CacheSessionSubsystem();

	if (!SessionSubsystem)
	{
		SetStatusMessage(FText::FromString(TEXT("Session subsystem is not available.")));
		OnCreateRoomResult(false);
		return;
	}

	if (HostButton)
	{
		HostButton->SetIsEnabled(false);
	}

	SetStatusMessage(FText::FromString(TEXT("Creating room...")));
	SessionSubsystem->CreateRoom(MaxPlayers, RoomName, MapName, MatchType);
}

void UMainMenuWidget::HostRoomWithOptions(int32 InMaxPlayers, const FString& InRoomName, const FString& InMapName, const FString& InMatchType)
{
	MaxPlayers = FMath::Max(1, InMaxPlayers);
	RoomName = InRoomName;
	MapName = InMapName.IsEmpty() ? TEXT("BombMap") : InMapName;
	MatchType = InMatchType.IsEmpty() ? TEXT("FreeForAll") : InMatchType;

	HostRoom();
}

void UMainMenuWidget::RefreshRooms()
{
	CacheSessionSubsystem();

	if (!SessionSubsystem)
	{
		SetStatusMessage(FText::FromString(TEXT("Session subsystem is not available.")));
		OnFindRoomsResult(TArray<FRoomInfo>(), false);
		return;
	}

	if (JoinButton)
	{
		JoinButton->SetIsEnabled(false);
	}

	if (RoomListArea)
	{
		RoomListArea->SetVisibility(ESlateVisibility::Visible);
	}

	SetStatusMessage(FText::FromString(TEXT("Searching rooms...")));
	SessionSubsystem->FindRooms();
}

void UMainMenuWidget::JoinRoom(int32 SessionIndex)
{
	CacheSessionSubsystem();

	if (!SessionSubsystem)
	{
		SetStatusMessage(FText::FromString(TEXT("Session subsystem is not available.")));
		OnJoinRoomResult(false);
		return;
	}

	SetStatusMessage(FText::FromString(TEXT("Joining room...")));
	SessionSubsystem->JoinRoomByIndex(SessionIndex);
}

void UMainMenuWidget::JoinLocalHost()
{
	APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!PlayerController)
	{
		SetStatusMessage(FText::FromString(TEXT("Local player controller is not available.")));
		OnJoinRoomResult(false);
		return;
	}

	SetStatusMessage(FText::FromString(TEXT("Joining local host...")));
	MenuTearDown();
	RemoveFromParent();
	PlayerController->ClientTravel(TEXT("127.0.0.1"), ETravelType::TRAVEL_Absolute);
	OnJoinRoomResult(true);
}

bool UMainMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}

	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}

	if (LocalJoinButton)
	{
		LocalJoinButton->OnClicked.AddDynamic(this, &ThisClass::LocalJoinButtonClicked);
	}

	if (CreateButton)
	{
		CreateButton->OnClicked.AddDynamic(this, &ThisClass::CreateButtonClicked);
	}

	if (CancelCreateButton)
	{
		CancelCreateButton->OnClicked.AddDynamic(this, &ThisClass::CancelCreateButtonClicked);
	}

	HideCreateRoomPanel();

	if (RoomListArea)
	{
		RoomListArea->SetVisibility(ESlateVisibility::Collapsed);
	}

	BindSessionDelegates();
	return true;
}

void UMainMenuWidget::NativeDestruct()
{
	UnbindSessionDelegates();
	Super::NativeDestruct();
}

void UMainMenuWidget::HostButtonClicked()
{
	ShowCreateRoomPanel();
}

void UMainMenuWidget::JoinButtonClicked()
{
	RefreshRooms();
}

void UMainMenuWidget::LocalJoinButtonClicked()
{
	JoinLocalHost();
}

void UMainMenuWidget::CreateButtonClicked()
{
	const FString InputRoomName = RoomNameInput ? RoomNameInput->GetText().ToString() : FString();
	const int32 InputMaxPlayers = MaxPlayersSpinBox ? FMath::RoundToInt(MaxPlayersSpinBox->GetValue()) : MaxPlayers;

	HideCreateRoomPanel();
	HostRoomWithOptions(InputMaxPlayers, InputRoomName, MapName, MatchType);
}

void UMainMenuWidget::CancelCreateButtonClicked()
{
	HideCreateRoomPanel();
	SetStatusMessage(FText::GetEmpty());
}

void UMainMenuWidget::HandleCreateRoomComplete(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		MenuTearDown();
		RemoveFromParent();
	}
	else if (HostButton)
	{
		HostButton->SetIsEnabled(true);
		SetStatusMessage(FText::FromString(TEXT("Failed to create room.")));
	}

	OnCreateRoomResult(bWasSuccessful);
}

void UMainMenuWidget::HandleFindRoomsComplete(const TArray<FRoomInfo>& Rooms, bool bWasSuccessful)
{
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(true);
	}

	if (bWasSuccessful)
	{
		RebuildRoomList(Rooms);

		if (Rooms.IsEmpty())
		{
			SetStatusMessage(FText::FromString(TEXT("No rooms found.")));
		}
		else
		{
			SetStatusMessage(FText::FromString(FString::Printf(TEXT("Found %d room(s)."), Rooms.Num())));
		}
	}
	else
	{
		SetStatusMessage(FText::FromString(TEXT("Failed to find rooms.")));
	}

	OnFindRoomsResult(Rooms, bWasSuccessful);
}

void UMainMenuWidget::HandleJoinRoomComplete(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		MenuTearDown();
		RemoveFromParent();
	}
	else
	{
		SetStatusMessage(FText::FromString(TEXT("Failed to join room.")));
	}

	OnJoinRoomResult(bWasSuccessful);
}

void UMainMenuWidget::HandleLeaveRoomComplete(bool bWasSuccessful)
{
	OnLeaveRoomResult(bWasSuccessful);
}

void UMainMenuWidget::BindSessionDelegates()
{
	CacheSessionSubsystem();

	if (bSessionDelegatesBound || !SessionSubsystem)
	{
		return;
	}

	SessionSubsystem->OnCreateRoomComplete.AddDynamic(this, &ThisClass::HandleCreateRoomComplete);
	SessionSubsystem->OnFindRoomsComplete.AddDynamic(this, &ThisClass::HandleFindRoomsComplete);
	SessionSubsystem->OnJoinRoomComplete.AddDynamic(this, &ThisClass::HandleJoinRoomComplete);
	SessionSubsystem->OnLeaveRoomComplete.AddDynamic(this, &ThisClass::HandleLeaveRoomComplete);

	bSessionDelegatesBound = true;
}

void UMainMenuWidget::UnbindSessionDelegates()
{
	if (!bSessionDelegatesBound || !SessionSubsystem)
	{
		return;
	}

	SessionSubsystem->OnCreateRoomComplete.RemoveDynamic(this, &ThisClass::HandleCreateRoomComplete);
	SessionSubsystem->OnFindRoomsComplete.RemoveDynamic(this, &ThisClass::HandleFindRoomsComplete);
	SessionSubsystem->OnJoinRoomComplete.RemoveDynamic(this, &ThisClass::HandleJoinRoomComplete);
	SessionSubsystem->OnLeaveRoomComplete.RemoveDynamic(this, &ThisClass::HandleLeaveRoomComplete);

	bSessionDelegatesBound = false;
}

void UMainMenuWidget::RebuildRoomList(const TArray<FRoomInfo>& Rooms)
{
	if (!RoomListBox)
	{
		return;
	}

	RoomListBox->ClearChildren();

	if (!RoomEntryWidgetClass)
	{
		return;
	}

	for (const FRoomInfo& Room : Rooms)
	{
		URoomEntryWidget* RoomEntry = CreateWidget<URoomEntryWidget>(this, RoomEntryWidgetClass);
		if (!RoomEntry)
		{
			continue;
		}

		RoomEntry->SetupRoomEntry(Room, this);
		RoomListBox->AddChild(RoomEntry);
	}
}

void UMainMenuWidget::ShowCreateRoomPanel()
{
	if (CreateRoomPanel)
	{
		CreateRoomPanel->SetVisibility(ESlateVisibility::Visible);
	}

	SetStatusMessage(FText::GetEmpty());
}

void UMainMenuWidget::HideCreateRoomPanel()
{
	if (CreateRoomPanel)
	{
		CreateRoomPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}
