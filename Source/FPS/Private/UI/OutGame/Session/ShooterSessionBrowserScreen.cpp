// ShooterSessionBrowserScreen.cpp
#include "UI/OutGame/Session/ShooterSessionBrowserScreen.h"

#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Session/SessionSubSystem.h"
#include "Tags/UITags.h"
#include "UI/Foundation/ShooterButtonBase.h"
#include "UI/OutGame/Session/ShooterRoomListEntry.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterSessionBrowser, Log, All);

void UShooterSessionBrowserScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Btn_Refresh->OnClicked().AddUObject(this, &ThisClass::HandleRefreshClicked);
	Btn_CreateRoom->OnClicked().AddUObject(this, &ThisClass::HandleCreateRoomClicked);
	Btn_Back->OnClicked().AddUObject(this, &ThisClass::HandleBackClicked);

	if (IsValid(Btn_LocalJoin))
	{
		Btn_LocalJoin->OnClicked().AddUObject(this, &ThisClass::HandleLocalJoinClicked);
	}
}

void UShooterSessionBrowserScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (USessionSubSystem* Session = GetSessionSubsystem())
	{
		Session->OnFindRoomsComplete.AddDynamic(this, &ThisClass::HandleFindRoomsComplete);
		Session->OnJoinRoomComplete.AddDynamic(this, &ThisClass::HandleJoinRoomComplete);
	}

	ScrollBox_Rooms->ClearChildren();
	StartFindRooms();
}

void UShooterSessionBrowserScreen::NativeOnDeactivated()
{
	if (USessionSubSystem* Session = GetSessionSubsystem())
	{
		Session->OnFindRoomsComplete.RemoveDynamic(this, &ThisClass::HandleFindRoomsComplete);
		Session->OnJoinRoomComplete.RemoveDynamic(this, &ThisClass::HandleJoinRoomComplete);
	}

	Super::NativeOnDeactivated();
}

UWidget* UShooterSessionBrowserScreen::NativeGetDesiredFocusTarget() const
{
	return Btn_Refresh;
}

bool UShooterSessionBrowserScreen::NativeOnHandleBackAction()
{
	if (bIsBusy)
	{
		return true;
	}

	return Super::NativeOnHandleBackAction();
}

void UShooterSessionBrowserScreen::HandleRefreshClicked()
{
	StartFindRooms();
}

void UShooterSessionBrowserScreen::HandleCreateRoomClicked()
{
	const TSubclassOf<UCommonActivatableWidget> LoadedClass = CreateRoomDialogClass.LoadSynchronous();
	const UGameInstance* GameInstance = GetGameInstance();
	UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
	if (!IsValid(UIManager) || !IsValid(LoadedClass))
	{
		UE_LOG(LogShooterSessionBrowser, Error, TEXT("CreateRoomDialogClass is not configured."));
		return;
	}

	UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, LoadedClass);
}

void UShooterSessionBrowserScreen::HandleBackClicked()
{
	DeactivateWidget();
}

void UShooterSessionBrowserScreen::HandleLocalJoinClicked()
{
	if (bIsBusy)
	{
		return;
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->ClientTravel(TEXT("127.0.0.1"), ETravelType::TRAVEL_Absolute);
	}
}

void UShooterSessionBrowserScreen::HandleRoomClicked(const int32 SessionIndex)
{
	USessionSubSystem* Session = GetSessionSubsystem();
	if (!IsValid(Session) || bIsBusy)
	{
		return;
	}

	SetBusy(true, NSLOCTEXT("ShooterLobby", "JoiningRoom", "참가하는 중"));
	Session->JoinRoomByIndex(SessionIndex);
}

void UShooterSessionBrowserScreen::HandleFindRoomsComplete(const TArray<FRoomInfo>& Rooms, const bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		SetBusy(false, NSLOCTEXT("ShooterLobby", "FindRoomsFailed", "방 목록을 불러오지 못했습니다"));
		return;
	}

	RebuildRoomList(Rooms);
	SetBusy(false, Rooms.IsEmpty()
		? NSLOCTEXT("ShooterLobby", "NoRooms", "방이 없습니다")
		: FText::GetEmpty());
}

void UShooterSessionBrowserScreen::HandleJoinRoomComplete(const bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		return;
	}

	SetBusy(false, NSLOCTEXT("ShooterLobby", "JoinRoomFailed", "참가하지 못했습니다"));
}

USessionSubSystem* UShooterSessionBrowserScreen::GetSessionSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return IsValid(GameInstance)
		? GameInstance->GetSubsystem<USessionSubSystem>()
		: nullptr;
}

void UShooterSessionBrowserScreen::StartFindRooms()
{
	USessionSubSystem* Session = GetSessionSubsystem();
	if (!IsValid(Session))
	{
		return;
	}

	ScrollBox_Rooms->ClearChildren();
	SetBusy(true, NSLOCTEXT("ShooterLobby", "SearchingRooms", "방을 찾는 중"));
	Session->FindRooms();
}

void UShooterSessionBrowserScreen::RebuildRoomList(const TArray<FRoomInfo>& Rooms)
{
	ScrollBox_Rooms->ClearChildren();

	if (!RoomEntryWidgetClass)
	{
		UE_LOG(LogShooterSessionBrowser, Error, TEXT("RoomEntryWidgetClass is not configured."));
		return;
	}

	for (const FRoomInfo& Info : Rooms)
	{
		UShooterRoomListEntry* Entry = CreateWidget<UShooterRoomListEntry>(this, RoomEntryWidgetClass);
		if (!IsValid(Entry))
		{
			continue;
		}

		Entry->SetRoomInfo(Info);
		Entry->OnClicked().AddUObject(this, &ThisClass::HandleRoomClicked, Info.SessionIndex);
		ScrollBox_Rooms->AddChild(Entry);
	}
}

void UShooterSessionBrowserScreen::SetBusy(const bool bNewBusy, const FText& StatusText)
{
	// 취소·입력칸도 잠금을 위함
	bIsBusy = bNewBusy;
	
	Btn_Refresh->SetIsEnabled(!bNewBusy);
	Btn_CreateRoom->SetIsEnabled(!bNewBusy);
	Btn_Back->SetIsEnabled(!bNewBusy);
	if (IsValid(Btn_LocalJoin))
	{
		Btn_LocalJoin->SetIsEnabled(!bNewBusy);
	}

	ScrollBox_Rooms->SetIsEnabled(!bNewBusy);

	Text_Status->SetText(StatusText);
	BP_ApplyStatus(!StatusText.IsEmpty());
}
