// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/MenuWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerController.h"
#include "Session/SessionSubSystem.h"
#include "UI/RoomEntryWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogMenuWidget, Log, All);

void UMenuWidget::MenuSetup(int32 InMaxPlayers, const FString& InMapName, const FString& InMatchType)
{
	UE_LOG(LogMenuWidget, Log, TEXT("MenuSetup: Widget=%s MapName=%s MatchType=%s"), *GetNameSafe(this), *InMapName, *InMatchType);

	MaxPlayers = InMaxPlayers;
	MapName = InMapName.IsEmpty() ? TEXT("BombMap") : InMapName;
	MatchType = InMatchType.IsEmpty() ? TEXT("FreeForAll") : InMatchType;

	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	CacheSessionSubsystem();
	BindSessionDelegates();
}

void UMenuWidget::HostRoom()
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

void UMenuWidget::HostRoomWithOptions(int32 InMaxPlayers, const FString& InRoomName, const FString& InMapName, const FString& InMatchType)
{
	MaxPlayers = FMath::Max(1, InMaxPlayers);
	RoomName = InRoomName;
	MapName = InMapName.IsEmpty() ? TEXT("BombMap") : InMapName;
	MatchType = InMatchType.IsEmpty() ? TEXT("FreeForAll") : InMatchType;

	HostRoom();
}

void UMenuWidget::RefreshRooms()
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

void UMenuWidget::JoinRoom(int32 SessionIndex)
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

void UMenuWidget::JoinLocalHost()
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

void UMenuWidget::SetLobbyReady(bool bNewLobbyReady)
{
	AShooterPlayerController* PlayerController = GetGameInstance()
		? Cast<AShooterPlayerController>(GetGameInstance()->GetFirstLocalPlayerController())
		: nullptr;
	if (!PlayerController)
	{
		SetStatusMessage(FText::FromString(TEXT("Player controller is not available.")));
		return;
	}

	PlayerController->SetLobbyReady(bNewLobbyReady);
	SetStatusMessage(bNewLobbyReady
		? FText::FromString(TEXT("Ready."))
		: FText::FromString(TEXT("Ready canceled.")));
}

void UMenuWidget::StartRoomGame()
{
	CacheSessionSubsystem();

	if (!SessionSubsystem)
	{
		SetStatusMessage(FText::FromString(TEXT("Session subsystem is not available.")));
		OnStartRoomGameResult(false);
		return;
	}

	SetStatusMessage(FText::FromString(TEXT("Starting game...")));
	SessionSubsystem->StartRoomGame(MapName);
}

bool UMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (HostButton)
	{
		UE_LOG(LogMenuWidget, Log, TEXT("Initialize: Bind HostButton on %s"), *GetNameSafe(this));
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}

	if (JoinButton)
	{
		UE_LOG(LogMenuWidget, Log, TEXT("Initialize: Bind JoinButton on %s"), *GetNameSafe(this));
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}

	if (LocalJoinButton)
	{
		UE_LOG(LogMenuWidget, Log, TEXT("Initialize: Bind LocalJoinButton on %s"), *GetNameSafe(this));
		LocalJoinButton->OnClicked.AddDynamic(this, &ThisClass::LocalJoinButtonClicked);
	}

	if (ReadyButton)
	{
		UE_LOG(LogMenuWidget, Log, TEXT("Initialize: Bind ReadyButton on %s"), *GetNameSafe(this));
		ReadyButton->OnClicked.AddDynamic(this, &ThisClass::ReadyButtonClicked);
	}
	else
	{
		UE_LOG(LogMenuWidget, Warning, TEXT("Initialize: ReadyButton is not bound on %s"), *GetNameSafe(this));
	}

	if (StartButton)
	{
		UE_LOG(LogMenuWidget, Log, TEXT("Initialize: Bind StartButton on %s"), *GetNameSafe(this));
		StartButton->OnClicked.AddDynamic(this, &ThisClass::StartButtonClicked);
	}
	else
	{
		UE_LOG(LogMenuWidget, Warning, TEXT("Initialize: StartButton is not bound on %s"), *GetNameSafe(this));
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

	CacheSessionSubsystem();
	BindSessionDelegates();

	return true;
}

void UMenuWidget::NativeDestruct()
{
	UnbindSessionDelegates();
	MenuTearDown();

	Super::NativeDestruct();
}

void UMenuWidget::HostButtonClicked()
{
	ShowCreateRoomPanel();
}

void UMenuWidget::JoinButtonClicked()
{
	RefreshRooms();
}

void UMenuWidget::LocalJoinButtonClicked()
{
	JoinLocalHost();
}

void UMenuWidget::ReadyButtonClicked()
{
	UE_LOG(LogMenuWidget, Log, TEXT("ReadyButtonClicked: Widget=%s"), *GetNameSafe(this));
	SetLobbyReady(true);

	if (ReadyButton)
	{
		ReadyButton->SetIsEnabled(false);
	}
}

void UMenuWidget::StartButtonClicked()
{
	UE_LOG(LogMenuWidget, Log, TEXT("StartButtonClicked: Widget=%s"), *GetNameSafe(this));
	StartRoomGame();
}

void UMenuWidget::CreateButtonClicked()
{
	const FString InputRoomName = RoomNameInput ? RoomNameInput->GetText().ToString() : FString();
	const int32 InputMaxPlayers = MaxPlayersSpinBox ? FMath::RoundToInt(MaxPlayersSpinBox->GetValue()) : MaxPlayers;

	HideCreateRoomPanel();
	HostRoomWithOptions(InputMaxPlayers, InputRoomName, MapName, MatchType);
}

void UMenuWidget::CancelCreateButtonClicked()
{
	HideCreateRoomPanel();
	SetStatusMessage(FText::GetEmpty());
}

void UMenuWidget::HandleCreateRoomComplete(bool bWasSuccessful)
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

void UMenuWidget::HandleFindRoomsComplete(const TArray<FRoomInfo>& Rooms, bool bWasSuccessful)
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

void UMenuWidget::HandleJoinRoomComplete(bool bWasSuccessful)
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

void UMenuWidget::HandleLeaveRoomComplete(bool bWasSuccessful)
{
	OnLeaveRoomResult(bWasSuccessful);
}

void UMenuWidget::HandleStartRoomGameComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		SetStatusMessage(FText::FromString(TEXT("Cannot start until every client is ready.")));
	}

	OnStartRoomGameResult(bWasSuccessful);
}

void UMenuWidget::CacheSessionSubsystem()
{
	if (!SessionSubsystem)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			SessionSubsystem = GameInstance->GetSubsystem<USessionSubSystem>();
			UE_LOG(LogMenuWidget, Log, TEXT("CacheSessionSubsystem: %s"), *GetNameSafe(SessionSubsystem));
		}
	}
}

void UMenuWidget::BindSessionDelegates()
{
	if (bSessionDelegatesBound || !SessionSubsystem)
	{
		return;
	}

	SessionSubsystem->OnCreateRoomComplete.AddDynamic(this, &ThisClass::HandleCreateRoomComplete);
	SessionSubsystem->OnFindRoomsComplete.AddDynamic(this, &ThisClass::HandleFindRoomsComplete);
	SessionSubsystem->OnJoinRoomComplete.AddDynamic(this, &ThisClass::HandleJoinRoomComplete);
	SessionSubsystem->OnLeaveRoomComplete.AddDynamic(this, &ThisClass::HandleLeaveRoomComplete);
	SessionSubsystem->OnStartRoomGameComplete.AddDynamic(this, &ThisClass::HandleStartRoomGameComplete);

	bSessionDelegatesBound = true;
}

void UMenuWidget::UnbindSessionDelegates()
{
	if (!bSessionDelegatesBound || !SessionSubsystem)
	{
		return;
	}

	SessionSubsystem->OnCreateRoomComplete.RemoveDynamic(this, &ThisClass::HandleCreateRoomComplete);
	SessionSubsystem->OnFindRoomsComplete.RemoveDynamic(this, &ThisClass::HandleFindRoomsComplete);
	SessionSubsystem->OnJoinRoomComplete.RemoveDynamic(this, &ThisClass::HandleJoinRoomComplete);
	SessionSubsystem->OnLeaveRoomComplete.RemoveDynamic(this, &ThisClass::HandleLeaveRoomComplete);
	SessionSubsystem->OnStartRoomGameComplete.RemoveDynamic(this, &ThisClass::HandleStartRoomGameComplete);

	bSessionDelegatesBound = false;
}

void UMenuWidget::RebuildRoomList(const TArray<FRoomInfo>& Rooms)
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

void UMenuWidget::ShowCreateRoomPanel()
{
	if (CreateRoomPanel)
	{
		CreateRoomPanel->SetVisibility(ESlateVisibility::Visible);
	}

	SetStatusMessage(FText::GetEmpty());
}

void UMenuWidget::HideCreateRoomPanel()
{
	if (CreateRoomPanel)
	{
		CreateRoomPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMenuWidget::SetStatusMessage(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}

	OnMenuStatusMessage(Message);
}

void UMenuWidget::MenuTearDown()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			const FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}
