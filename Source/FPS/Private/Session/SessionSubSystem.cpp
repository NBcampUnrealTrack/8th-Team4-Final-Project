// Fill out your copyright notice in the Description page of Project Settings.


#include "Session/SessionSubSystem.h"

#include "Engine/Engine.h"
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
#include "TimerManager.h"

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

	// Steam OSS 는 문자열 설정을 ANSI_TO_TCHAR 로 읽어 UTF-8 바이트가 0xFF80 이상으로 퍼진다. 그 모양일 때만 되돌린다
	FString RestoreSteamUtf8(const FString& Value)
	{
		TArray<uint8> Bytes;
		Bytes.Reserve(Value.Len());
		bool bHasSpreadByte = false;
		for (const TCHAR Ch : Value)
		{
			if (Ch < 0x80)
			{
				Bytes.Add(static_cast<uint8>(Ch));
			}
			else if (Ch >= 0xFF80)
			{
				Bytes.Add(static_cast<uint8>(Ch & 0xFF));
				bHasSpreadByte = true;
			}
			else
			{
				// 정상 유니코드 (NULL OSS 등)
				return Value;
			}
		}

		if (!bHasSpreadByte)
		{
			return Value;
		}

		const FString Restored = FString::ConstructFromPtrSize(
			reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());

		// 유효한 UTF-8 이 아니었으면 원본 유지
		const auto ReEncoded = StringCast<UTF8CHAR>(*Restored, Restored.Len());
		const bool bValidUtf8 = ReEncoded.Length() == Bytes.Num()
			&& FMemory::Memcmp(ReEncoded.Get(), Bytes.GetData(), Bytes.Num()) == 0;
		return bValidUtf8 ? Restored : Value;
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
	if (IsRoomOperationInProgress() || !EnsureSessionInterface())
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

	ApplyCachedRoomSettings(*LastSessionSettings, false);

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
	if (FindSessionsCompleteDelegateHandle.IsValid() || !EnsureSessionInterface())
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
		FindSessionsCompleteDelegateHandle.Reset();
		OnFindRoomsComplete.Broadcast(TArray<FRoomInfo>(), false);
		return;
	}

	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
		OnFindRoomsComplete.Broadcast(TArray<FRoomInfo>(), false);
	}
}

void USessionSubSystem::JoinRoomByIndex(int32 SessionIndex)
{
	if (IsRoomOperationInProgress() || !EnsureSessionInterface() || !LastSessionSearch.IsValid() || !LastSessionSearch->SearchResults.IsValidIndex(SessionIndex))
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
	bRoomSettingsUpdatePending = false;
	if (SessionInterface.IsValid() && UpdateSessionCompleteDelegateHandle.IsValid())
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
	UpdateSessionCompleteDelegateHandle.Reset();
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

	// OnlineSubsystem 자체가 없는 실행 환경에서도 Frontend로 돌아간다.
	if (!EnsureSessionInterface())
	{
		LeaveRoomContinuation = ELeaveRoomContinuation::None;
		OnLeaveRoomComplete.Broadcast(true);
		ReturnToFrontendNow();
		return;
	}
	LeaveRoom();
}

bool USessionSubSystem::LeaveRoomAndOpenLocalMap(
	const TSoftObjectPtr<UWorld>& DestinationMap, const bool bPrepareCombatAssets)
{
	UWorld* World = GetWorld();
	const UGameInstance* GameInstance = GetGameInstance();
	if (DestinationMap.IsNull() || !World || World->GetNetMode() == NM_DedicatedServer
		|| !GameInstance || GameInstance->GetLocalPlayers().Num() != 1 || IsRoomOperationInProgress()) return false;
	EnsureSessionInterface();
	PendingLocalMap = DestinationMap;
	bPrepareLocalCombatAssets = bPrepareCombatAssets;
	LeaveRoomContinuation = ELeaveRoomContinuation::OpenLocalMap;
	CachedJoinSessionIndex = INDEX_NONE;
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession)) CompleteLeaveRoom(true);
	else LeaveRoom();
	return true;
}


bool USessionSubSystem::ReturnRoomToLobby()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client || IsRoomOperationInProgress())
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
		EndSessionCompleteDelegateHandle.Reset();
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
		|| bReturnRoomToLobbyPending || bCancelRoomGameStartPending
		|| TravelOperation != ETravelOperation::None)
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

	QueueRoomSettingsUpdate();

	return true;
}

void USessionSubSystem::StartRoomGame(const FString& GameMapName)
{
	StartRoomGameWithSettings(GameMapName, TEXT(""));
}

void USessionSubSystem::StartRoomGameWithSettings(const FString& GameMapName, const FString& GameMatchType)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || IsRoomOperationInProgress()
		|| !EnsureSessionInterface() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		OnStartRoomGameComplete.Broadcast(false);
		return;
	}
	const EOnlineSessionState::Type State = SessionInterface->GetSessionState(NAME_GameSession);
	if (State != EOnlineSessionState::Pending && State != EOnlineSessionState::Ended)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Session] Start rejected in state %s"), SessionStateToString(State));
		OnStartRoomGameComplete.Broadcast(false);
		return;
	}
	if (!GameMapName.IsEmpty()) { CachedMapName = GameMapName; CachedMapPath = ResolveMapPathFromName(GameMapName); }
	if (!GameMatchType.IsEmpty()) CachedMatchType = GameMatchType;
	SetRoomJoinable(false);
	bStartRoomGamePending = true;
	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);
	if (!SessionInterface->StartSession(NAME_GameSession) && bStartRoomGamePending)
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		StartSessionCompleteDelegateHandle.Reset();
		bStartRoomGamePending = false;
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
		RoomInfo.RoomName = RestoreSteamUtf8(RoomInfo.RoomName);
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
	// 게임모드 BeginPlay만으로는 아직 로딩 중인 참가자가 준비됐다고 볼 수 없다.
	if (bIsJoinable && TravelOperation == ETravelOperation::MatchLoading && bConnectionGraceActive) return false;
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

	QueueRoomSettingsUpdate();

	return true;
}

void USessionSubSystem::NotifyRoomMembersChanged()
{
	// Steam 로비 인원은 접속·종료 이벤트보다 늦게 반영될 수 있어 잠깐 뒤에 게시한다
	GetGameInstance()->GetTimerManager().SetTimer(
		RoomMembersPublishHandle, this, &ThisClass::PublishRoomMembers, 2.0f, false);
}

void USessionSubSystem::PublishRoomMembers()
{
	// 방 이동·시작 처리 중이면 끝날 때까지 미룬다
	if (IsRoomOperationInProgress())
	{
		NotifyRoomMembersChanged();
		return;
	}

	QueueRoomSettingsUpdate();
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
	if (SessionName != NAME_GameSession || !CreateSessionCompleteDelegateHandle.IsValid()) return;
	if (SessionInterface) SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	CreateSessionCompleteDelegateHandle.Reset();
	if (!bWasSuccessful) { OnCreateRoomComplete.Broadcast(false); return; }
	UWorld* World = GetWorld();
	if (!World || !GEngine)
	{
		ResetTravelOperation();
		bCleanupFailedClientSession = true;
		EnsureTravelTicker();
		OnCreateRoomComplete.Broadcast(false);
		return;
	}

	// 미완료 접속이 남으면 새 리슨 월드도 클라이언트로 판정해 GameMode 생성을 건너뛸 수 있다.
	GEngine->CancelPending(World, nullptr);
	TravelOperation = ETravelOperation::HostLobby;
	TravelSourceWorld = World;
	BeginTravelConnectionGrace(LobbyMapPath);
	// 새 방은 이전 서버 주소와 이동 옵션을 이어받지 않고 처음부터 리슨 서버로 연다.
	UGameplayStatics::OpenLevel(World, FName(*LobbyMapPath), true, TEXT("listen"));
	// 로비 리슨 월드가 준비된 뒤 NotifyLobbyReady에서 공개하고 완료를 알린다.
}


void USessionSubSystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface) SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	FindSessionsCompleteDelegateHandle.Reset();
	OnFindRoomsComplete.Broadcast(bWasSuccessful && LastSessionSearch.IsValid() ? BuildRoomInfoList() : TArray<FRoomInfo>(),
		bWasSuccessful && LastSessionSearch.IsValid());
}


void USessionSubSystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionName != NAME_GameSession || !JoinSessionCompleteDelegateHandle.IsValid()) return;
	if (SessionInterface) SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	JoinSessionCompleteDelegateHandle.Reset();
	FString ConnectString;
	APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (Result != EOnJoinSessionCompleteResult::Success || !PC || !SessionInterface.IsValid()
		|| !SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		bCleanupFailedClientSession = SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession);
		EnsureTravelTicker();
		OnJoinRoomComplete.Broadcast(false);
		return;
	}
	TravelOperation = ETravelOperation::JoinRoom;
	TravelSourceWorld = GetWorld();
	BeginTravelConnectionGrace(FString());
	PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
	// 접속 요청의 성공과 도착 완료는 분리한다.
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
	LeaveRoomContinuation = ELeaveRoomContinuation::None;
	if (!bWasSuccessful)
	{
		CachedJoinSessionIndex = INDEX_NONE;
		if (Continuation == ELeaveRoomContinuation::OpenLocalMap) { FailLocalMapTravel(TEXT("DestroySession failed")); return; }
		PendingLocalMap.Reset();
		if (Continuation == ELeaveRoomContinuation::CreateRoom) OnCreateRoomComplete.Broadcast(false);
		else if (Continuation == ELeaveRoomContinuation::JoinRoom) OnJoinRoomComplete.Broadcast(false);
		else OnLeaveRoomComplete.Broadcast(false);
		return;
	}
	switch (Continuation)
	{
	case ELeaveRoomContinuation::CreateRoom:
		CreateRoom(CachedMaxPlayers, CachedRoomName, CachedMapName, CachedMatchType); return;
	case ELeaveRoomContinuation::JoinRoom:
	{
		const int32 Index = CachedJoinSessionIndex;
		CachedJoinSessionIndex = INDEX_NONE;
		JoinRoomByIndex(Index); return;
	}
	case ELeaveRoomContinuation::ReturnToFrontend:
		ResetTravelOperation(); CloseTravelPresentation();
		OnLeaveRoomComplete.Broadcast(true); ReturnToFrontendNow(); return;
	case ELeaveRoomContinuation::OpenLocalMap:
		StartLocalMapPreparation(); return;
	default:
		OnLeaveRoomComplete.Broadcast(true); return;
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
	if (!IsValid(World) || World->GetNetMode() == NM_Client) return false;
	SetRoomJoinable(false);
	TravelOperation = ETravelOperation::ReturningLobby;
	TravelSourceWorld = World;
	BeginTravelConnectionGrace(LobbyMapPath);
	if (World->ServerTravel(LobbyMapPath + TEXT("?listen"))) return true;
	ResetTravelOperation();
	UE_LOG(LogTemp, Warning, TEXT("[Session] Lobby travel was rejected"));
	return false;
}


void USessionSubSystem::OnEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession || !EndSessionCompleteDelegateHandle.IsValid()) return;
	if (SessionInterface) SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
	EndSessionCompleteDelegateHandle.Reset();
	const bool bReturnToLobby = bReturnRoomToLobbyPending;
	const bool bCancelledStart = bCancelRoomGameStartPending;
	bReturnRoomToLobbyPending = false;
	bCancelRoomGameStartPending = false;
	const EOnlineSessionState::Type State = SessionInterface.IsValid()
		? SessionInterface->GetSessionState(NAME_GameSession) : EOnlineSessionState::NoSession;
	if (!bWasSuccessful && State != EOnlineSessionState::Ended)
	{
		UE_LOG(LogTemp, Error, TEXT("[Session] EndSession failed in state %s; retry remains blocked by OSS state"), SessionStateToString(State));
		return;
	}
	if (bCancelledStart) SetRoomJoinable(true);
	else if (bReturnToLobby) TravelRoomToLobbyNow();
}


void USessionSubSystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession || !StartSessionCompleteDelegateHandle.IsValid()) return;
	if (SessionInterface) SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	StartSessionCompleteDelegateHandle.Reset();
	bStartRoomGamePending = false;
	if (bCancelRoomGameStartPending)
	{
		bCancelRoomGameStartPending = false;
		CancelRoomGameStart();
		return;
	}
	if (!bWasSuccessful) SetRoomJoinable(true);
	OnStartRoomGameComplete.Broadcast(bWasSuccessful);
}


void USessionSubSystem::Deinitialize()
{
	ShutdownTravelLifecycle();
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

	FindSessionsCompleteDelegateHandle.Reset();
	DestroySessionCompleteDelegateHandle.Reset();
	StartSessionCompleteDelegateHandle.Reset();
	EndSessionCompleteDelegateHandle.Reset();
	LastSessionSettings.Reset();
	PendingLocalMap.Reset();
	LeaveRoomContinuation = ELeaveRoomContinuation::None;
	LastSessionSearch.Reset();
	SessionInterface.Reset();

	Super::Deinitialize();
}
