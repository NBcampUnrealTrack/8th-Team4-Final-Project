// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Net/Core/Connection/NetEnums.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Session/RoomInfo.h"
#include "SessionSubSystem.generated.h"

class UNetDriver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLocalMapTravelComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCreateRoomComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFindRoomsComplete, const TArray<FRoomInfo>&, Rooms, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJoinRoomComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaveRoomComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStartRoomGameComplete, bool, bWasSuccessful);

UCLASS()
class FPS_API USessionSubSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
	
public:
	USessionSubSystem();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable)
	void CreateRoom(int32 MaxPlayers, const FString& RoomName, const FString& MapName, const FString& MatchType);

	UFUNCTION(BlueprintCallable)
	void FindRooms();

	UFUNCTION(BlueprintCallable)
	void JoinRoomByIndex(int32 SessionIndex);

	UFUNCTION(BlueprintCallable)
	void LeaveRoom();

	// 현재 Named Session을 정리한 뒤 Frontend(Game Default Map)로 돌아간다.
	// 세션 종료는 비동기이므로 이동은 DestroySession 완료 뒤에 수행한다.
	void LeaveRoomAndReturnToFrontend();

	bool LeaveRoomAndOpenLocalMap(const TSoftObjectPtr<UWorld>& DestinationMap, bool bPrepareCombatAssets = true);

	// 리슨 서버가 현재 방을 유지한 채 공유 로비 맵으로 돌아간다.
	// 일반 클라이언트에서는 호출할 수 없다.
	bool ReturnRoomToLobby();

	// Keeps room search metadata aligned with the host's lobby selection.
	// Selection authority itself is validated by LobbyGameMode.
	UFUNCTION(BlueprintCallable)
	bool UpdateRoomGameMode(const FString& GameMapName, const FString& GameMatchType);

	UFUNCTION(BlueprintCallable)
	void StartRoomGame(const FString& GameMapName = TEXT(""));

	// LobbyGameMode uses this path to commit map and match type together.
	void StartRoomGameWithSettings(const FString& GameMapName, const FString& GameMatchType);

	// 게임 모드가 실제 플레이 맵에 진입한 뒤 중도 참가 허용 여부를 명시적으로 제어한다.
	bool SetRoomJoinable(bool bIsJoinable);

	void NotifyLobbyReady(UWorld* LobbyWorld);
	void CancelRoomGameStart();
	bool IsRoomOperationInProgress() const;
	void BeginTravelConnectionGrace(const FString& DestinationMap);
	void EndTravelConnectionGrace();

	UPROPERTY(BlueprintAssignable)
	FOnLocalMapTravelComplete OnLocalMapTravelComplete;

	const FString& GetRoomName() const;
	int32 GetMaxPlayers() const;

	UPROPERTY(BlueprintAssignable)
	FOnCreateRoomComplete OnCreateRoomComplete;

	UPROPERTY(BlueprintAssignable)
	FOnFindRoomsComplete OnFindRoomsComplete;

	UPROPERTY(BlueprintAssignable)
	FOnJoinRoomComplete OnJoinRoomComplete;

	UPROPERTY(BlueprintAssignable)
	FOnLeaveRoomComplete OnLeaveRoomComplete;

	UPROPERTY(BlueprintAssignable)
	FOnStartRoomGameComplete OnStartRoomGameComplete;

protected:
	virtual void Deinitialize() override;

private:
	bool EnsureSessionInterface();

	FString BuildDefaultRoomName() const;
	FString ResolveMapPathFromName(const FString& MapName) const;
	TArray<FRoomInfo> BuildRoomInfoList() const;
	void ApplyCachedRoomSettings(FOnlineSessionSettings& SessionSettings, bool bIsJoinable) const;
	void CompleteLeaveRoom(bool bWasSuccessful);
	void ReturnToFrontendNow();
	bool TravelRoomToLobbyNow();
	void QueueRoomSettingsUpdate();
	void SendRoomSettingsUpdate();
	void OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	void ShutdownTravelLifecycle();
	void StartLocalMapPreparation();
	void ContinueLocalMapTravel();
	void FailLocalMapTravel(const FString& Reason);
	void CloseTravelPresentation();
	void EnsureTravelTicker();
	bool TickTravel(float DeltaTime);
	void HandleNetDriverCreated(UWorld* World, UNetDriver* NetDriver);
	void ApplyTravelConnectionGrace(UNetDriver* NetDriver);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	bool IsDestinationWorldReady() const;
	void ResetTravelOperation();

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnEndSessionComplete(FName SessionName, bool bWasSuccessful);

private:
	enum class ELeaveRoomContinuation : uint8
	{
		None,
		CreateRoom,
		JoinRoom,
		ReturnToFrontend,
		OpenLocalMap
	};

	enum class ETravelOperation : uint8
	{
		None,
		HostLobby,
		JoinRoom,
		LocalMapPreparing,
		LocalMapLoading,
		MatchLoading,
		ReturningLobby
	};

	struct FConnectionTimeoutSnapshot
	{
		TWeakObjectPtr<UNetDriver> Driver;
		float ConnectionTimeout = 0.0f;
		float InitialConnectTimeout = 0.0f;
	};

	ETravelOperation TravelOperation = ETravelOperation::None;
	FString TravelDestination;
	TWeakObjectPtr<UWorld> TravelSourceWorld;
	double TravelStartedAt = 0.0;
	bool bConnectionGraceActive = false;
	bool bCleanupFailedClientSession = false;
	bool bPrepareLocalCombatAssets = true;
	bool bRoomSettingsUpdatePending = false;
	uint32 RoomSettingsRevision = 0;
	uint32 SubmittedRoomSettingsRevision = 0;
	double NextRoomSettingsUpdateAt = 0.0;
	FDelegateHandle UpdateSessionCompleteDelegateHandle;
	bool bCancelRoomGameStartPending = false;
	TArray<FConnectionTimeoutSnapshot> ConnectionTimeoutSnapshots;
	FTSTicker::FDelegateHandle TravelTickerHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle NetDriverCreatedHandle;

	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;

	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;

	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;

	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;

	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;

	FOnEndSessionCompleteDelegate EndSessionCompleteDelegate;
	FDelegateHandle EndSessionCompleteDelegateHandle;

	ELeaveRoomContinuation LeaveRoomContinuation = ELeaveRoomContinuation::None;
	bool bStartRoomGamePending = false;
	bool bReturnRoomToLobbyPending = false;
	int32 CachedJoinSessionIndex = INDEX_NONE;
	TSoftObjectPtr<UWorld> PendingLocalMap;

	int32 CachedMaxPlayers = 4;
	FString CachedRoomName = TEXT("Player's Room");
	FString CachedMapName = TEXT("DominationMap");
	FString CachedMatchType = TEXT("Domination");
	FString CachedMapPath = TEXT("/Game/Maps/DominationMap");

	UPROPERTY(EditDefaultsOnly, Category = "Session|Travel")
	FString LobbyMapPath = TEXT("/Game/Maps/Lobby");
};
