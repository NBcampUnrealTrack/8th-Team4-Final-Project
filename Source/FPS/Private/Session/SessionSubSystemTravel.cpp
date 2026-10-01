#include "Session/SessionSubSystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameMode/LobbyGameMode.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Misc/PackageName.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Preload/DominationPreloadSubsystem.h"
#include "Travel/ShooterTravelSettings.h"
#include "UI/Transition/ScreenTransitionSubsystem.h"

namespace
{
	FString TravelMapPackage(const FString& MapPath)
	{
		FString Package = MapPath;
		int32 OptionsIndex = INDEX_NONE;
		if (Package.FindChar(TEXT('?'), OptionsIndex)) Package.LeftInline(OptionsIndex);
		return UWorld::RemovePIEPrefix(FPackageName::ObjectPathToPackageName(Package));
	}

	bool IsMainTravelDriver(const UNetDriver* Driver)
	{
		return Driver && (Driver->NetDriverName == NAME_GameNetDriver
			|| Driver->NetDriverName == FName(TEXT("PendingNetDriver")));
	}
}

void USessionSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDominationPreloadSubsystem>();
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
	NetDriverCreatedHandle = FWorldDelegates::OnNetDriverCreated.AddUObject(this, &ThisClass::HandleNetDriverCreated);
}

void USessionSubSystem::ShutdownTravelLifecycle()
{
	bRoomSettingsUpdatePending = false;
	if (SessionInterface.IsValid()) SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
	UpdateSessionCompleteDelegateHandle.Reset();
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	FWorldDelegates::OnNetDriverCreated.Remove(NetDriverCreatedHandle);
	if (TravelTickerHandle.IsValid()) FTSTicker::RemoveTicker(TravelTickerHandle);
	TravelTickerHandle.Reset();
	ResetTravelOperation();
}

bool USessionSubSystem::IsRoomOperationInProgress() const
{
	if (TravelOperation != ETravelOperation::None || !PendingLocalMap.IsNull()
		|| LeaveRoomContinuation != ELeaveRoomContinuation::None || bCleanupFailedClientSession
		|| bStartRoomGamePending || bReturnRoomToLobbyPending || bCancelRoomGameStartPending
		|| CreateSessionCompleteDelegateHandle.IsValid() || JoinSessionCompleteDelegateHandle.IsValid()
		|| DestroySessionCompleteDelegateHandle.IsValid() || EndSessionCompleteDelegateHandle.IsValid()) return true;
	if (!SessionInterface.IsValid()) return false;
	const EOnlineSessionState::Type State = SessionInterface->GetSessionState(NAME_GameSession);
	return State == EOnlineSessionState::Creating || State == EOnlineSessionState::Starting
		|| State == EOnlineSessionState::Ending || State == EOnlineSessionState::Destroying;
}

void USessionSubSystem::CancelRoomGameStart()
{
	if (!SessionInterface.IsValid()) return;
	const EOnlineSessionState::Type State = SessionInterface->GetSessionState(NAME_GameSession);
	if (bStartRoomGamePending || State == EOnlineSessionState::Starting)
	{
		// 아직 도착하지 않은 Start 콜백은 성공해도 이동을 시작하지 않는다.
		bCancelRoomGameStartPending = true;
		return;
	}
	if (EndSessionCompleteDelegateHandle.IsValid()) return;
	if (State != EOnlineSessionState::InProgress)
	{
		if (State == EOnlineSessionState::Pending || State == EOnlineSessionState::Ended) SetRoomJoinable(true);
		return;
	}
	bCancelRoomGameStartPending = true;
	EndSessionCompleteDelegateHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegate);
	if (!SessionInterface->EndSession(NAME_GameSession) && EndSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		EndSessionCompleteDelegateHandle.Reset();
		bCancelRoomGameStartPending = false;
		if (SessionInterface->GetSessionState(NAME_GameSession) == EOnlineSessionState::Ended) SetRoomJoinable(true);
		else UE_LOG(LogTemp, Error, TEXT("[Session] Could not roll back cancelled match start"));
	}
}

void USessionSubSystem::QueueRoomSettingsUpdate()
{
	++RoomSettingsRevision;
	bRoomSettingsUpdatePending = true;
	NextRoomSettingsUpdateAt = 0.0;
	if (!UpdateSessionCompleteDelegateHandle.IsValid()) SendRoomSettingsUpdate();
	EnsureTravelTicker();
}

void USessionSubSystem::SendRoomSettingsUpdate()
{
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{ bRoomSettingsUpdatePending = false; return; }
	bRoomSettingsUpdatePending = false;
	SubmittedRoomSettingsRevision = RoomSettingsRevision;
	UpdateSessionCompleteDelegateHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnUpdateSessionComplete));
	if (!SessionInterface->UpdateSession(NAME_GameSession,
		SessionInterface->GetNamedSession(NAME_GameSession)->SessionSettings, true)
		&& UpdateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		UpdateSessionCompleteDelegateHandle.Reset();
		bRoomSettingsUpdatePending = true;
		NextRoomSettingsUpdateAt = FPlatformTime::Seconds() + 2.0;
	}
}

void USessionSubSystem::OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession || !UpdateSessionCompleteDelegateHandle.IsValid()) return;
	if (SessionInterface.IsValid()) SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
	UpdateSessionCompleteDelegateHandle.Reset();
	bRoomSettingsUpdatePending = !bWasSuccessful || SubmittedRoomSettingsRevision != RoomSettingsRevision;
	if (!bWasSuccessful)
		UE_LOG(LogTemp, Warning, TEXT("[Session] Room settings publication failed; latest settings will be retried"));
	if (bRoomSettingsUpdatePending)
	{
		NextRoomSettingsUpdateAt = FPlatformTime::Seconds() + (bWasSuccessful ? 0.0 : 2.0);
		EnsureTravelTicker();
	}
}

void USessionSubSystem::NotifyLobbyReady(UWorld* LobbyWorld)
{
	if (!LobbyWorld || LobbyWorld->GetGameInstance() != GetGameInstance() || !LobbyWorld->HasBegunPlay()
		|| (LobbyWorld->GetNetMode() != NM_ListenServer && LobbyWorld->GetNetMode() != NM_DedicatedServer)
		|| !IsMainTravelDriver(LobbyWorld->GetNetDriver()) || !LobbyWorld->GetAuthGameMode<ALobbyGameMode>()) return;
	const ETravelOperation CompletedOperation = TravelOperation;
	if (!IsDestinationWorldReady()) return;
	if (CompletedOperation != ETravelOperation::HostLobby && CompletedOperation != ETravelOperation::ReturningLobby) return;
	// 공유 로비가 실제로 열린 시점에 검색/참가를 허용한다.
	ResetTravelOperation();
	CloseTravelPresentation();
	SetRoomJoinable(true);
	if (CompletedOperation == ETravelOperation::HostLobby) OnCreateRoomComplete.Broadcast(true);
}

void USessionSubSystem::BeginTravelConnectionGrace(const FString& DestinationMap)
{
	const bool bCompletedJoin = TravelOperation == ETravelOperation::JoinRoom && !DestinationMap.IsEmpty();
	if (bCompletedJoin)
	{
		// 호스트의 준비 RPC를 이미 받았다면 참가 완료 poll과 새 START를 분리한다.
		ResetTravelOperation();
	}
	if (TravelOperation == ETravelOperation::None)
	{
		TravelOperation = ETravelOperation::MatchLoading;
		TravelSourceWorld = GetWorld();
	}
	TravelDestination = TravelMapPackage(DestinationMap);
	TravelStartedAt = FPlatformTime::Seconds();
	// 로딩 총 시간으로 유예를 해제하지 않는다. 실제 도착, 취소, 실패에서 복구한다.
	bConnectionGraceActive = true;
	if (UWorld* World = GetWorld()) ApplyTravelConnectionGrace(World->GetNetDriver());
	EnsureTravelTicker();
	// 완료 콜백이 재진입해도 새 START가 이미 진행 중인 상태로 보이게 한다.
	if (bCompletedJoin) OnJoinRoomComplete.Broadcast(true);
}

void USessionSubSystem::HandleNetDriverCreated(UWorld* World, UNetDriver* NetDriver)
{
	if (bConnectionGraceActive && World && World->GetGameInstance() == GetGameInstance()) ApplyTravelConnectionGrace(NetDriver);
}

void USessionSubSystem::ApplyTravelConnectionGrace(UNetDriver* NetDriver)
{
	if (!bConnectionGraceActive || !IsMainTravelDriver(NetDriver)) return;
	if (ConnectionTimeoutSnapshots.ContainsByPredicate([NetDriver](const FConnectionTimeoutSnapshot& Snapshot)
		{ return Snapshot.Driver.Get() == NetDriver; })) return;
	FConnectionTimeoutSnapshot Snapshot;
	Snapshot.Driver = NetDriver;
	Snapshot.ConnectionTimeout = NetDriver->ConnectionTimeout;
	Snapshot.InitialConnectTimeout = NetDriver->InitialConnectTimeout;
	ConnectionTimeoutSnapshots.Add(Snapshot);
	const float Grace = FMath::Max(1.0f, GetDefault<UShooterTravelSettings>()->LoadingConnectionTimeout);
	NetDriver->ConnectionTimeout = FMath::Max(NetDriver->ConnectionTimeout, Grace);
	NetDriver->InitialConnectTimeout = FMath::Max(NetDriver->InitialConnectTimeout, Grace);
	UE_LOG(LogTemp, Log, TEXT("[Travel] Connection grace Driver=%s Timeout=%.1f->%.1f Initial=%.1f->%.1f Target=%s"),
		*NetDriver->NetDriverName.ToString(), Snapshot.ConnectionTimeout, NetDriver->ConnectionTimeout,
		Snapshot.InitialConnectTimeout, NetDriver->InitialConnectTimeout, *TravelDestination);
}

void USessionSubSystem::EndTravelConnectionGrace()
{
	bConnectionGraceActive = false;
	for (const FConnectionTimeoutSnapshot& Snapshot : ConnectionTimeoutSnapshots)
	{
		if (UNetDriver* Driver = Snapshot.Driver.Get())
		{
			Driver->ConnectionTimeout = Snapshot.ConnectionTimeout;
			Driver->InitialConnectTimeout = Snapshot.InitialConnectTimeout;
			UE_LOG(LogTemp, Log, TEXT("[Travel] Restored connection timeouts Driver=%s Timeout=%.1f Initial=%.1f"),
				*Driver->NetDriverName.ToString(), Driver->ConnectionTimeout, Driver->InitialConnectTimeout);
		}
	}
	ConnectionTimeoutSnapshots.Reset();
	if (TravelOperation == ETravelOperation::MatchLoading)
	{
		TravelOperation = ETravelOperation::None;
		TravelDestination.Reset();
		TravelSourceWorld.Reset();
	}
}

void USessionSubSystem::ResetTravelOperation()
{
	EndTravelConnectionGrace();
	TravelOperation = ETravelOperation::None;
	TravelDestination.Reset();
	TravelSourceWorld.Reset();
}

void USessionSubSystem::EnsureTravelTicker()
{
	if (!TravelTickerHandle.IsValid())
		TravelTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickTravel), 0.1f);
}

void USessionSubSystem::StartLocalMapPreparation()
{
	if (PendingLocalMap.IsNull()) { FailLocalMapTravel(TEXT("Local destination is empty")); return; }
	TravelOperation = ETravelOperation::LocalMapPreparing;
	TravelSourceWorld = GetWorld();
	TravelDestination = PendingLocalMap.ToSoftObjectPath().GetLongPackageName();
	TravelStartedAt = FPlatformTime::Seconds();
	EnsureTravelTicker();
	if (!bPrepareLocalCombatAssets)
	{
		UScreenTransitionSubsystem* Transition = GetGameInstance()->GetSubsystem<UScreenTransitionSubsystem>();
		const FSimpleDelegate OpenMap = FSimpleDelegate::CreateUObject(this, &ThisClass::ContinueLocalMapTravel);
		if (!Transition || !Transition->BeginTransition(OpenMap)) OpenMap.ExecuteIfBound();
		return;
	}
	UDominationPreloadSubsystem* Preload = GetGameInstance()->GetSubsystem<UDominationPreloadSubsystem>();
	if (!Preload) { FailLocalMapTravel(TEXT("Preload subsystem is unavailable")); return; }
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetGameInstance()->GetFirstLocalPlayerController()))
		PC->ShowLoadingScreen(0.2f);
	Preload->PrepareForMap(TravelDestination);
}

void USessionSubSystem::ContinueLocalMapTravel()
{
	if (TravelOperation != ETravelOperation::LocalMapPreparing || PendingLocalMap.IsNull()) return;
	TravelOperation = ETravelOperation::LocalMapLoading;
	TravelStartedAt = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Log, TEXT("[Travel] Opening standalone map after preparation Target=%s"), *TravelDestination);
	// listen/options는 전달하지 않는다. 맵 도착은 ticker가 실제 월드에서 확인한다.
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*TravelDestination), true, FString());
}

void USessionSubSystem::CloseTravelPresentation()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULoadingScreenSubsystem* Loading = GI->GetSubsystem<ULoadingScreenSubsystem>()) Loading->HideLoadingScreen();
		if (UScreenTransitionSubsystem* Transition = GI->GetSubsystem<UScreenTransitionSubsystem>()) Transition->EndTransition();
	}
}

void USessionSubSystem::FailLocalMapTravel(const FString& Reason)
{
	UE_LOG(LogTemp, Error, TEXT("[Travel] Local map failed Target=%s Reason=%s"), *TravelDestination, *Reason);
	PendingLocalMap.Reset();
	ResetTravelOperation();
	if (UDominationPreloadSubsystem* Preload = GetGameInstance()->GetSubsystem<UDominationPreloadSubsystem>()) Preload->ReleasePreload();
	CloseTravelPresentation();
	OnLocalMapTravelComplete.Broadcast(false);
}

bool USessionSubSystem::IsDestinationWorldReady() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetGameInstance() != GetGameInstance() || !World->HasBegunPlay()) return false;
	if (!TravelDestination.IsEmpty()
		&& TravelMapPackage(World->GetOutermost()->GetName()) != TravelDestination) return false;
	if (TravelOperation == ETravelOperation::JoinRoom)
	{
		if (TravelSourceWorld.Get() == World || World->GetNetMode() != NM_Client) return false;
		APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
		if (!PC || !PC->PlayerState || !World->GetGameState()) return false;
		if (!World->GetGameState<ABaseTeamGameState>()) return true;
		const AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
		return PS && PS->IsLoadedIntoMatch();
	}
	if (TravelOperation == ETravelOperation::HostLobby || TravelOperation == ETravelOperation::ReturningLobby)
		return (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer)
			&& IsMainTravelDriver(World->GetNetDriver()) && World->GetAuthGameMode<ALobbyGameMode>();
	if (TravelOperation == ETravelOperation::LocalMapLoading && !bPrepareLocalCombatAssets)
		return TravelSourceWorld.Get() != World && World->GetNetMode() == NM_Standalone;
	if (!World->GetGameState<ABaseTeamGameState>()) return false;
	int32 PlayerCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		const AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
		if (!PS || !PS->IsLoadedIntoMatch()) return false;
		++PlayerCount;
	}
	return PlayerCount > 0;
}

bool USessionSubSystem::TickTravel(float DeltaTime)
{
	if (bCleanupFailedClientSession)
	{
		bCleanupFailedClientSession = false;
		// 엔진이 이미 예약한 ?closed 이동과 경쟁하지 않고 Named Session만 정리한다.
		LeaveRoomContinuation = ELeaveRoomContinuation::None;
		LeaveRoom();
	}
	if (bRoomSettingsUpdatePending && !UpdateSessionCompleteDelegateHandle.IsValid()
		&& FPlatformTime::Seconds() >= NextRoomSettingsUpdateAt) SendRoomSettingsUpdate();
	if (bConnectionGraceActive && GetWorld()) ApplyTravelConnectionGrace(GetWorld()->GetNetDriver());
	if (TravelOperation == ETravelOperation::LocalMapPreparing && bPrepareLocalCombatAssets)
	{
		UDominationPreloadSubsystem* Preload = GetGameInstance()->GetSubsystem<UDominationPreloadSubsystem>();
		if (!Preload || Preload->HasPreloadFailed()) FailLocalMapTravel(TEXT("Asset preparation failed"));
		else if (Preload->IsPreloadReady()) ContinueLocalMapTravel();
	}
	if (TravelOperation != ETravelOperation::None && IsDestinationWorldReady())
	{
		const ETravelOperation Completed = TravelOperation;
		const double Elapsed = FPlatformTime::Seconds() - TravelStartedAt;
		if (Completed == ETravelOperation::HostLobby || Completed == ETravelOperation::ReturningLobby) NotifyLobbyReady(GetWorld());
		else
		{
			ResetTravelOperation();
			if (Completed == ETravelOperation::JoinRoom) OnJoinRoomComplete.Broadcast(true);
			else if (Completed == ETravelOperation::LocalMapLoading)
			{
				PendingLocalMap.Reset();
				if (bPrepareLocalCombatAssets)
				{
					if (ULoadingScreenSubsystem* Loading = GetGameInstance()->GetSubsystem<ULoadingScreenSubsystem>())
						Loading->NotifyWorldLoaded();
				}
				else CloseTravelPresentation();
				OnLocalMapTravelComplete.Broadcast(true);
			}
			else if (Completed == ETravelOperation::MatchLoading && GetWorld()->GetNetMode() != NM_Client) SetRoomJoinable(true);
		}
		UE_LOG(LogTemp, Log, TEXT("[Travel] Destination ready after %.2fs"), Elapsed);
	}
	const bool bKeepTicking = TravelOperation != ETravelOperation::None || bCleanupFailedClientSession || bRoomSettingsUpdatePending;
	if (!bKeepTicking) TravelTickerHandle.Reset();
	return bKeepTicking;
}

void USessionSubSystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UWorld* FailureWorld = World ? World : (NetDriver ? NetDriver->GetWorld() : nullptr);
	if (!FailureWorld || FailureWorld->GetGameInstance() != GetGameInstance()) return;
	const bool bDriverFailure = FailureType == ENetworkFailure::NetDriverCreateFailure
		|| FailureType == ENetworkFailure::NetDriverListenFailure;
	if (!IsMainTravelDriver(NetDriver) && !(bDriverFailure && !NetDriver)) return;
	UE_LOG(LogTemp, Error, TEXT("[Travel] NetworkFailure=%d Mode=%d Driver=%s Target=%s Reason=%s"),
		int32(FailureType), int32(FailureWorld->GetNetMode()),
		NetDriver ? *NetDriver->NetDriverName.ToString() : TEXT("None"), *TravelDestination, *ErrorString);
	if (bDriverFailure)
	{
		// 리슨 드라이버 자체의 실패는 참가자 한 명의 연결 종료와 다르게 처리한다.
		HandleTravelFailure(FailureWorld, ETravelFailure::LoadMapFailure, ErrorString);
		SetRoomJoinable(false);
		if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
		{
			bCleanupFailedClientSession = true;
			EnsureTravelTicker();
		}
		return;
	}
	// 서버의 한 참가자 연결 실패로 호스트의 방 전체를 파괴하지 않는다.
	if (FailureWorld->GetNetMode() != NM_Client && NetDriver->NetDriverName != FName(TEXT("PendingNetDriver"))) return;
	const ETravelOperation Failed = TravelOperation;
	ResetTravelOperation();
	CloseTravelPresentation();
	if (UDominationPreloadSubsystem* Preload = GetGameInstance()->GetSubsystem<UDominationPreloadSubsystem>()) Preload->ReleasePreload();
	bCleanupFailedClientSession = true;
	EnsureTravelTicker();
	if (Failed == ETravelOperation::JoinRoom) OnJoinRoomComplete.Broadcast(false);
	else if (Failed == ETravelOperation::LocalMapPreparing || Failed == ETravelOperation::LocalMapLoading)
	{
		PendingLocalMap.Reset();
		OnLocalMapTravelComplete.Broadcast(false);
	}
}

void USessionSubSystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!World || World->GetGameInstance() != GetGameInstance()) return;
	UE_LOG(LogTemp, Error, TEXT("[Travel] TravelFailure=%d Target=%s Reason=%s"), int32(FailureType), *TravelDestination, *ErrorString);
	const ETravelOperation Failed = TravelOperation;
	if (ALobbyGameMode* Lobby = World->GetAuthGameMode<ALobbyGameMode>()) Lobby->CancelPendingMatchTravel(ErrorString);
	if (Failed == ETravelOperation::LocalMapPreparing || Failed == ETravelOperation::LocalMapLoading)
	{ FailLocalMapTravel(ErrorString); return; }
	ResetTravelOperation();
	CloseTravelPresentation();
	if (UDominationPreloadSubsystem* Preload = GetGameInstance()->GetSubsystem<UDominationPreloadSubsystem>()) Preload->ReleasePreload();
	if (Failed == ETravelOperation::HostLobby || Failed == ETravelOperation::JoinRoom || World->GetNetMode() == NM_Client)
	{
		bCleanupFailedClientSession = true;
		EnsureTravelTicker();
	}
	if (Failed == ETravelOperation::HostLobby) OnCreateRoomComplete.Broadcast(false);
	else if (Failed == ETravelOperation::JoinRoom) OnJoinRoomComplete.Broadcast(false);
}
