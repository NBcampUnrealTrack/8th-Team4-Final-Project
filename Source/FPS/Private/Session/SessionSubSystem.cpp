// Fill out your copyright notice in the Description page of Project Settings.


#include "Session/SessionSubSystem.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Player/ShooterPlayerState.h"

namespace SessionSettingKeys
{
	const FName RoomName(TEXT("RoomName"));
	const FName MapName(TEXT("MapName"));
	const FName MapPath(TEXT("MapPath"));
	const FName MatchType(TEXT("MatchType"));
}

USessionSubSystem::USessionSubSystem()
	: CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete))
	, FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete))
	, JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete))
	, DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete))
{
}

void USessionSubSystem::CreateRoom(int32 MaxPlayers, const FString& RoomName, const FString& MapName, const FString& MatchType)
{
	if (!EnsureSessionInterface())
	{
		OnCreateRoomComplete.Broadcast(false);
		return;
	}

	CachedMaxPlayers = MaxPlayers;
	CachedRoomName = RoomName.IsEmpty() ? BuildDefaultRoomName() : RoomName;
	CachedMapName = MapName.IsEmpty() ? TEXT("BombMap") : MapName;
	CachedMatchType = MatchType.IsEmpty() ? TEXT("FreeForAll") : MatchType;
	CachedMapPath = ResolveMapPathFromName(CachedMapName);

	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		bCreateRoomAfterDestroy = true;
		LeaveRoom();
		return;
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());

	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	const bool bIsNullSubsystem = Subsystem && Subsystem->GetSubsystemName() == TEXT("NULL");

	LastSessionSettings->bIsLANMatch = bIsNullSubsystem;
	LastSessionSettings->NumPublicConnections = CachedMaxPlayers;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->BuildUniqueId = 1;

	LastSessionSettings->Set(SessionSettingKeys::RoomName, CachedRoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(SessionSettingKeys::MapName, CachedMapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(SessionSettingKeys::MapPath, CachedMapPath, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(SessionSettingKeys::MatchType, CachedMatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		OnCreateRoomComplete.Broadcast(false);
		return;
	}

	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		OnCreateRoomComplete.Broadcast(false);
	}
}

void USessionSubSystem::FindRooms()
{
	if (!EnsureSessionInterface())
	{
		OnFindRoomsComplete.Broadcast(TArray<FRoomInfo>(), false);
		return;
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = 10000;

	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	const bool bIsNullSubsystem = Subsystem && Subsystem->GetSubsystemName() == TEXT("NULL");
	LastSessionSearch->bIsLanQuery = bIsNullSubsystem;

	if (!bIsNullSubsystem)
	{
		LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}

	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		OnFindRoomsComplete.Broadcast(TArray<FRoomInfo>(), false);
		return;
	}

	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		OnFindRoomsComplete.Broadcast(TArray<FRoomInfo>(), false);
	}
}

void USessionSubSystem::JoinRoomByIndex(int32 SessionIndex)
{
	if (!EnsureSessionInterface() || !LastSessionSearch.IsValid() || !LastSessionSearch->SearchResults.IsValidIndex(SessionIndex))
	{
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, LastSessionSearch->SearchResults[SessionIndex]))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		OnJoinRoomComplete.Broadcast(false);
	}
}

void USessionSubSystem::LeaveRoom()
{
	if (!EnsureSessionInterface())
	{
		OnLeaveRoomComplete.Broadcast(false);
		return;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		OnLeaveRoomComplete.Broadcast(true);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		OnLeaveRoomComplete.Broadcast(false);
	}
}

void USessionSubSystem::StartRoomGame(const FString& GameMapName)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		OnStartRoomGameComplete.Broadcast(false);
		return;
	}

	if (!CanStartRoomGame())
	{
		OnStartRoomGameComplete.Broadcast(false);
		return;
	}

	if (!GameMapName.IsEmpty())
	{
		CachedMapName = GameMapName;
		CachedMapPath = ResolveMapPathFromName(CachedMapName);
	}

	UE_LOG(LogTemp, Log, TEXT("[SessionSubSystem] StartRoomGame: ServerTravel %s"), *CachedMapPath);

	OnStartRoomGameComplete.Broadcast(true);
	World->ServerTravel(CachedMapPath + TEXT("?listen"));
}

bool USessionSubSystem::CanStartRoomGame() const
{
	UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client && AreAllClientsReady();
}

bool USessionSubSystem::EnsureSessionInterface()
{
	if (!SessionInterface)
	{
		if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld()))
		{
			SessionInterface = Subsystem->GetSessionInterface();
		}
	}

	return SessionInterface.IsValid();
}

FString USessionSubSystem::BuildDefaultRoomName() const
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (Subsystem)
	{
		const IOnlineIdentityPtr IdentityInterface = Subsystem->GetIdentityInterface();
		if (IdentityInterface.IsValid())
		{
			const FString PlayerName = IdentityInterface->GetPlayerNickname(0);
			if (!PlayerName.IsEmpty())
			{
				return FString::Printf(TEXT("%s's Room"), *PlayerName);
			}
		}
	}

	return TEXT("Player's Room");
}

FString USessionSubSystem::ResolveMapPathFromName(const FString& MapName) const
{
	if (MapName.StartsWith(TEXT("/Game/")))
	{
		return MapName;
	}

	if (MapName.IsEmpty())
	{
		return TEXT("/Game/Maps/BombMap");
	}

	return FString::Printf(TEXT("/Game/Maps/%s"), *MapName);
}

TArray<FRoomInfo> USessionSubSystem::BuildRoomInfoList() const
{
	TArray<FRoomInfo> Rooms;

	if (!LastSessionSearch.IsValid())
	{
		return Rooms;
	}

	for (int32 Index = 0; Index < LastSessionSearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Result = LastSessionSearch->SearchResults[Index];

		FRoomInfo RoomInfo;
		RoomInfo.SessionIndex = Index;
		RoomInfo.PingInMs = Result.PingInMs;
		RoomInfo.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
		RoomInfo.CurrentPlayers = FMath::Max(0, RoomInfo.MaxPlayers - Result.Session.NumOpenPublicConnections);

		Result.Session.SessionSettings.Get(SessionSettingKeys::RoomName, RoomInfo.RoomName);
		Result.Session.SessionSettings.Get(SessionSettingKeys::MapName, RoomInfo.MapName);
		Result.Session.SessionSettings.Get(SessionSettingKeys::MatchType, RoomInfo.MatchType);

		if (RoomInfo.RoomName.IsEmpty())
		{
			RoomInfo.RoomName = TEXT("Unknown Room");
		}

		if (RoomInfo.MapName.IsEmpty())
		{
			RoomInfo.MapName = TEXT("Unknown Map");
		}

		if (RoomInfo.MatchType.IsEmpty())
		{
			RoomInfo.MatchType = TEXT("Unknown");
		}

		Rooms.Add(RoomInfo);
	}

	return Rooms;
}

bool USessionSubSystem::AreAllClientsReady() const
{
	UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return false;
	}

	bool bHasClientPlayer = false;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (!IsValid(PlayerState) || IsHostPlayerState(PlayerState))
		{
			continue;
		}

		bHasClientPlayer = true;
		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (!IsValid(ShooterPlayerState) || !ShooterPlayerState->IsLobbyReady())
		{
			return false;
		}
	}

	return bHasClientPlayer;
}

bool USessionSubSystem::IsHostPlayerState(const APlayerState* PlayerState) const
{
	UWorld* World = GetWorld();
	const APlayerController* HostController = World ? World->GetFirstPlayerController() : nullptr;
	return IsValid(PlayerState) && IsValid(HostController) && HostController->PlayerState == PlayerState;
}

void USessionSubSystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	OnCreateRoomComplete.Broadcast(bWasSuccessful);

	if (bWasSuccessful)
	{
		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(LobbyMapPath + TEXT("?listen"));
		}
	}
}

void USessionSubSystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	if (!bWasSuccessful || !LastSessionSearch.IsValid())
	{
		OnFindRoomsComplete.Broadcast(TArray<FRoomInfo>(), false);
		return;
	}

	OnFindRoomsComplete.Broadcast(BuildRoomInfoList(), true);
}

void USessionSubSystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	FString ConnectString;
	if (!SessionInterface.IsValid() || !SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!PlayerController)
	{
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	PlayerController->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
	OnJoinRoomComplete.Broadcast(true);
}

void USessionSubSystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	if (bWasSuccessful && bCreateRoomAfterDestroy)
	{
		bCreateRoomAfterDestroy = false;
		CreateRoom(CachedMaxPlayers, CachedRoomName, CachedMapName, CachedMatchType);
		return;
	}

	OnLeaveRoomComplete.Broadcast(bWasSuccessful);
}

void USessionSubSystem::Deinitialize()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	LastSessionSettings.Reset();
	LastSessionSearch.Reset();
	SessionInterface.Reset();

	Super::Deinitialize();
}
