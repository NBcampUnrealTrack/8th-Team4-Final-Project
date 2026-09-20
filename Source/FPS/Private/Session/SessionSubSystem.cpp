// Fill out your copyright notice in the Description page of Project Settings.


#include "Session/SessionSubSystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

namespace SessionSettingKeys
{
	const FName RoomName(TEXT("RoomName"));
	const FName MapName(TEXT("MapName"));
	const FName MapPath(TEXT("MapPath"));
	const FName MatchType(TEXT("MatchType"));
	const FName Joinable(TEXT("Joinable"));
}

namespace
{
	const TCHAR* SessionStateToString(const EOnlineSessionState::Type SessionState)
	{
		switch (SessionState)
		{
		case EOnlineSessionState::NoSession:
			return TEXT("NoSession");
		case EOnlineSessionState::Creating:
			return TEXT("Creating");
		case EOnlineSessionState::Pending:
			return TEXT("Pending");
		case EOnlineSessionState::Starting:
			return TEXT("Starting");
		case EOnlineSessionState::InProgress:
			return TEXT("InProgress");
		case EOnlineSessionState::Ending:
			return TEXT("Ending");
		case EOnlineSessionState::Ended:
			return TEXT("Ended");
		case EOnlineSessionState::Destroying:
			return TEXT("Destroying");
		default:
			return TEXT("Unknown");
		}
	}
}

USessionSubSystem::USessionSubSystem()
	: CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete))
	, FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete))
	, JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete))
	, DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete))
	, StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete))
	, EndSessionCompleteDelegate(FOnEndSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnEndSessionComplete))
{
}

void USessionSubSystem::CreateRoom(int32 MaxPlayers, const FString& RoomName, const FString& MapName, const FString& MatchType)
{
	if (!PendingLocalMap.IsNull() || !EnsureSessionInterface())
	{
		OnCreateRoomComplete.Broadcast(false);
		return;
	}

	CachedMaxPlayers = MaxPlayers;
	CachedRoomName = RoomName.IsEmpty() ? BuildDefaultRoomName() : RoomName;
	CachedMapName = MapName.IsEmpty() ? TEXT("DominationMap") : MapName;
	CachedMatchType = MatchType.IsEmpty() ? TEXT("Domination") : MatchType;
	CachedMapPath = ResolveMapPathFromName(CachedMapName);

	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		LeaveRoomContinuation = ELeaveRoomContinuation::CreateRoom;
		CachedJoinSessionIndex = INDEX_NONE;
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

	ApplyCachedRoomSettings(*LastSessionSettings, true);

	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
		OnCreateRoomComplete.Broadcast(false);
		return;
	}

	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
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
	if (!PendingLocalMap.IsNull() || !EnsureSessionInterface() || !LastSessionSearch.IsValid() || !LastSessionSearch->SearchResults.IsValidIndex(SessionIndex))
	{
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	bool bIsJoinable = true;
	LastSessionSearch->SearchResults[SessionIndex].Session.SessionSettings.Get(SessionSettingKeys::Joinable, bIsJoinable);
	if (!bIsJoinable)
	{
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	// 이전 방을 떠나지 않은 상태에서 같은 NAME_GameSession으로 JoinSession을 호출하면
	// OnlineSubsystem이 기존 Named Session 때문에 거부한다. 먼저 정상적으로 정리한 뒤 재시도한다.
	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		CachedJoinSessionIndex = SessionIndex;
		LeaveRoomContinuation = ELeaveRoomContinuation::JoinRoom;
		LeaveRoom();
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, LastSessionSearch->SearchResults[SessionIndex]))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
		OnJoinRoomComplete.Broadcast(false);
	}
}

void USessionSubSystem::LeaveRoom()
{
	if (DestroySessionCompleteDelegateHandle.IsValid())
	{
		return;
	}

	if (!EnsureSessionInterface())
	{
		CompleteLeaveRoom(false);
		return;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		CompleteLeaveRoom(true);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
		CompleteLeaveRoom(false);
	}
}

void USessionSubSystem::LeaveRoomAndReturnToFrontend()
{
	if (!PendingLocalMap.IsNull())
	{
		return;
	}

	LeaveRoomContinuation = ELeaveRoomContinuation::ReturnToFrontend;
	CachedJoinSessionIndex = INDEX_NONE;

	// OnlineSubsystem 자체가 없는 실행 환경에서는 정리할 Named Session도 없다.
	// Frontend 이동까지 막지는 않는다.
	if (!EnsureSessionInterface())
	{
		LeaveRoomContinuation = ELeaveRoomContinuation::None;
		OnLeaveRoomComplete.Broadcast(true);
		ReturnToFrontendNow();
		return;
	}

	LeaveRoom();
}

bool USessionSubSystem::LeaveRoomAndOpenLocalMap(const TSoftObjectPtr<UWorld>& DestinationMap)
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = GetGameInstance();
	if (DestinationMap.IsNull() || !World || World->GetNetMode() == NM_DedicatedServer
		|| !GameInstance || GameInstance->GetLocalPlayers().Num() != 1
		|| bStartRoomGamePending || bReturnRoomToLobbyPending
		|| LeaveRoomContinuation != ELeaveRoomContinuation::None
		|| DestroySessionCompleteDelegateHandle.IsValid()
		|| CreateSessionCompleteDelegateHandle.IsValid() || JoinSessionCompleteDelegateHandle.IsValid())
	{
		return false;
	}

	if (EnsureSessionInterface())
	{
		const EOnlineSessionState::Type State = SessionInterface->GetSessionState(NAME_GameSession);
		if (State == EOnlineSessionState::Creating || State == EOnlineSessionState::Starting
			|| State == EOnlineSessionState::Ending || State == EOnlineSessionState::Destroying)
		{
			return false;
		}
	}

	PendingLocalMap = DestinationMap;
	LeaveRoomContinuation = ELeaveRoomContinuation::OpenLocalMap;
	CachedJoinSessionIndex = INDEX_NONE;
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		CompleteLeaveRoom(true);
	}
	else
	{
		LeaveRoom();
	}
	return true;
}

bool USessionSubSystem::ReturnRoomToLobby()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client || bReturnRoomToLobbyPending || !PendingLocalMap.IsNull())
	{
		return false;
	}

	// OnlineSubsystem이 없는 로컬 실행은 세션 수명주기 없이 맵만 이동한다.
	if (!EnsureSessionInterface() || SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		return TravelRoomToLobbyNow();
	}

	const EOnlineSessionState::Type SessionState = SessionInterface->GetSessionState(NAME_GameSession);
	if (SessionState == EOnlineSessionState::InProgress)
	{
		// 같은 방을 유지하되 현재 매치만 종료한다. 다음 로비 Start에서
		// Ended -> StartSession -> InProgress 순서를 다시 밟을 수 있게 한다.
		bReturnRoomToLobbyPending = true;
		EndSessionCompleteDelegateHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegate);
		if (SessionInterface->EndSession(NAME_GameSession))
		{
			UE_LOG(LogTemp, Log, TEXT("[SessionSubSystem] Ending active session before returning to lobby."));
			return true;
		}

		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		bReturnRoomToLobbyPending = false;

		// 동기 실패 직전에 상태가 이미 Ended로 바뀐 구현체는 정상 완료로 취급한다.
		if (SessionInterface->GetSessionState(NAME_GameSession) == EOnlineSessionState::Ended)
		{
			return TravelRoomToLobbyNow();
		}

		UE_LOG(LogTemp, Warning, TEXT("[SessionSubSystem] EndSession could not begin; lobby travel was cancelled."));
		return false;
	}

	if (SessionState == EOnlineSessionState::Pending || SessionState == EOnlineSessionState::Ended)
	{
		return TravelRoomToLobbyNow();
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[SessionSubSystem] ReturnRoomToLobby blocked by session state %s."),
		SessionStateToString(SessionState));
	return false;
}

bool USessionSubSystem::UpdateRoomGameMode(
	const FString& GameMapName,
	const FString& GameMatchType)
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| World->GetNetMode() == NM_Client
		|| GameMapName.IsEmpty()
		|| GameMatchType.IsEmpty()
		|| !EnsureSessionInterface())
	{
		return false;
	}

	FNamedOnlineSession* NamedSession =
		SessionInterface->GetNamedSession(NAME_GameSession);
	if (NamedSession == nullptr
		|| bStartRoomGamePending
		|| bReturnRoomToLobbyPending)
	{
		return false;
	}

	CachedMapName = GameMapName;
	CachedMapPath = ResolveMapPathFromName(CachedMapName);
	CachedMatchType = GameMatchType;

	bool bIsJoinable = NamedSession->SessionSettings.bShouldAdvertise;
	NamedSession->SessionSettings.Get(
		SessionSettingKeys::Joinable,
		bIsJoinable);

	ApplyCachedRoomSettings(NamedSession->SessionSettings, bIsJoinable);
	if (LastSessionSettings.IsValid())
	{
		ApplyCachedRoomSettings(*LastSessionSettings, bIsJoinable);
	}

	if (!SessionInterface->UpdateSession(
		NAME_GameSession,
		NamedSession->SessionSettings,
		true))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SessionSubSystem] Failed to advertise game mode. Map=%s MatchType=%s"),
			*CachedMapName,
			*CachedMatchType);
		return false;
	}

	return true;
}

void USessionSubSystem::StartRoomGame(const FString& GameMapName)
{
	StartRoomGameWithSettings(GameMapName, TEXT(""));
}

void USessionSubSystem::StartRoomGameWithSettings(
	const FString& GameMapName,
	const FString& GameMatchType)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SessionSubSystem] StartRoomGame blocked: world is invalid or this is a client."));
		OnStartRoomGameComplete.Broadcast(false);
		return;
	}

	if (!EnsureSessionInterface()
		|| SessionInterface->GetNamedSession(NAME_GameSession) == nullptr
		|| !PendingLocalMap.IsNull()
		|| bStartRoomGamePending
		|| bReturnRoomToLobbyPending)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SessionSubSystem] StartRoomGame blocked: SessionInterface=%s NamedSession=%s StartPending=%s LobbyReturnPending=%s"),
			SessionInterface.IsValid() ? TEXT("Valid") : TEXT("Invalid"),
			SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession) ? TEXT("Valid") : TEXT("None"),
			bStartRoomGamePending ? TEXT("true") : TEXT("false"),
			bReturnRoomToLobbyPending ? TEXT("true") : TEXT("false"));
		OnStartRoomGameComplete.Broadcast(false);
		return;
	}

	if (!GameMapName.IsEmpty())
	{
		CachedMapName = GameMapName;
		CachedMapPath = ResolveMapPathFromName(CachedMapName);
	}

	if (!GameMatchType.IsEmpty())
	{
		CachedMatchType = GameMatchType;
	}

	SetRoomJoinable(false);

	UE_LOG(LogTemp, Log,
		TEXT("[SessionSubSystem] StartRoomGame: StartSession before travel. Target=%s MatchType=%s"),
		*CachedMapPath,
		*CachedMatchType);

	bStartRoomGamePending = true;
	StartSessionCompleteDelegateHandle =
		SessionInterface->AddOnStartSessionCompleteDelegate_Handle(
			StartSessionCompleteDelegate);

	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		const EOnlineSessionState::Type SessionState =
			SessionInterface->GetSessionState(NAME_GameSession);
		UE_LOG(LogTemp, Warning,
			TEXT("[SessionSubSystem] StartSession returned false. State=%s Target=%s"),
			SessionStateToString(SessionState),
			*CachedMapPath);

		bStartRoomGamePending = false;
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(
			StartSessionCompleteDelegateHandle);

		// A new match must successfully transition Pending/Ended -> InProgress.
		SetRoomJoinable(true);
		OnStartRoomGameComplete.Broadcast(false);
	}
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
		return TEXT("/Game/Maps/DominationMap");
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
		Result.Session.SessionSettings.Get(SessionSettingKeys::Joinable, RoomInfo.bIsJoinable);

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

void USessionSubSystem::ApplyCachedRoomSettings(
	FOnlineSessionSettings& SessionSettings,
	const bool bIsJoinable) const
{
	SessionSettings.bAllowJoinInProgress = bIsJoinable;
	SessionSettings.bAllowJoinViaPresence = bIsJoinable;
	SessionSettings.bShouldAdvertise = bIsJoinable;
	SessionSettings.Set(
		SessionSettingKeys::RoomName,
		CachedRoomName,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(
		SessionSettingKeys::MapName,
		CachedMapName,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(
		SessionSettingKeys::MapPath,
		CachedMapPath,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(
		SessionSettingKeys::MatchType,
		CachedMatchType,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(
		SessionSettingKeys::Joinable,
		bIsJoinable,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
}

bool USessionSubSystem::SetRoomJoinable(const bool bIsJoinable)
{
	if (!EnsureSessionInterface())
	{
		return false;
	}

	FNamedOnlineSession* NamedSession =
		SessionInterface->GetNamedSession(NAME_GameSession);
	if (!NamedSession)
	{
		return false;
	}

	ApplyCachedRoomSettings(NamedSession->SessionSettings, bIsJoinable);
	if (LastSessionSettings.IsValid())
	{
		ApplyCachedRoomSettings(*LastSessionSettings, bIsJoinable);
	}

	if (!SessionInterface->UpdateSession(
		NAME_GameSession,
		NamedSession->SessionSettings,
		true))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SessionSubSystem] Failed to update room joinability. Joinable=%s"),
			bIsJoinable ? TEXT("true") : TEXT("false"));
		return false;
	}

	return true;
}

const FString& USessionSubSystem::GetRoomName() const
{
	return CachedRoomName;
}

int32 USessionSubSystem::GetMaxPlayers() const
{
	return CachedMaxPlayers;
}

void USessionSubSystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
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
		JoinSessionCompleteDelegateHandle.Reset();
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
	if (SessionName != NAME_GameSession)
	{
		return;
	}
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	DestroySessionCompleteDelegateHandle.Reset();

	CompleteLeaveRoom(bWasSuccessful);
}

void USessionSubSystem::CompleteLeaveRoom(const bool bWasSuccessful)
{
	const ELeaveRoomContinuation Continuation = LeaveRoomContinuation;
	const TSoftObjectPtr<UWorld> LocalMap = PendingLocalMap;
	PendingLocalMap.Reset();
	LeaveRoomContinuation = ELeaveRoomContinuation::None;

	if (!bWasSuccessful)
	{
		CachedJoinSessionIndex = INDEX_NONE;

		switch (Continuation)
		{
		case ELeaveRoomContinuation::CreateRoom:
			OnCreateRoomComplete.Broadcast(false);
			return;

		case ELeaveRoomContinuation::JoinRoom:
			OnJoinRoomComplete.Broadcast(false);
			return;

		default:
			OnLeaveRoomComplete.Broadcast(false);
			return;
		}
	}

	switch (Continuation)
	{
	case ELeaveRoomContinuation::CreateRoom:
		CreateRoom(CachedMaxPlayers, CachedRoomName, CachedMapName, CachedMatchType);
		return;

	case ELeaveRoomContinuation::JoinRoom:
	{
		const int32 SessionIndex = CachedJoinSessionIndex;
		CachedJoinSessionIndex = INDEX_NONE;
		JoinRoomByIndex(SessionIndex);
		return;
	}

	case ELeaveRoomContinuation::ReturnToFrontend:
		OnLeaveRoomComplete.Broadcast(true);
		ReturnToFrontendNow();
		return;

	case ELeaveRoomContinuation::OpenLocalMap:
		// 이전 방의 listen 옵션을 넘기지 않고 이 인스턴스만 이동한다.
		UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), LocalMap, true, FString());
		return;

	default:
		OnLeaveRoomComplete.Broadcast(true);
		return;
	}
}

void USessionSubSystem::ReturnToFrontendNow()
{
	UWorld* World = GetWorld();
	if (IsValid(World) && World->GetNetMode() != NM_Client)
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			// 리슨 호스트는 접속 중인 클라이언트도 정상적으로 Main Menu로 보낸다.
			GameMode->ReturnToMainMenuHost();
			return;
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->ReturnToMainMenu();
	}
}

bool USessionSubSystem::TravelRoomToLobbyNow()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	// 방 자체는 유지하므로 로비에서 다시 검색/참가할 수 있게 연다.
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		SetRoomJoinable(true);
	}

	UE_LOG(LogTemp, Log, TEXT("[SessionSubSystem] Returning room to lobby. SessionState=%s"),
		SessionInterface.IsValid()
			? SessionStateToString(SessionInterface->GetSessionState(NAME_GameSession))
			: TEXT("NoInterface"));

	return World->ServerTravel(LobbyMapPath + TEXT("?listen"));
}

void USessionSubSystem::OnEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
	}

	if (!bReturnRoomToLobbyPending)
	{
		return;
	}

	bReturnRoomToLobbyPending = false;
	const EOnlineSessionState::Type SessionState = SessionInterface.IsValid()
		? SessionInterface->GetSessionState(NAME_GameSession)
		: EOnlineSessionState::NoSession;

	if (!bWasSuccessful && SessionState != EOnlineSessionState::Ended)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SessionSubSystem] EndSession failed. Session=%s State=%s"),
			*SessionName.ToString(),
			SessionStateToString(SessionState));
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[SessionSubSystem] Session ended; continuing to lobby. Session=%s State=%s"),
		*SessionName.ToString(),
		SessionStateToString(SessionState));
	TravelRoomToLobbyNow();
}

void USessionSubSystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}

	bStartRoomGamePending = false;

	if (!bWasSuccessful)
	{
		const EOnlineSessionState::Type SessionState = SessionInterface.IsValid()
			? SessionInterface->GetSessionState(NAME_GameSession)
			: EOnlineSessionState::NoSession;
		UE_LOG(LogTemp, Warning,
			TEXT("[SessionSubSystem] StartSession failed. Session=%s State=%s Target=%s"),
			*SessionName.ToString(),
			SessionStateToString(SessionState),
			*CachedMapPath);

		SetRoomJoinable(true);
		OnStartRoomGameComplete.Broadcast(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SessionSubSystem] StartSession succeeded. Target=%s"), *CachedMapPath);
	OnStartRoomGameComplete.Broadcast(true);
}

void USessionSubSystem::Deinitialize()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
	}

	LastSessionSettings.Reset();
	PendingLocalMap.Reset();
	LeaveRoomContinuation = ELeaveRoomContinuation::None;
	LastSessionSearch.Reset();
	SessionInterface.Reset();

	Super::Deinitialize();
}
