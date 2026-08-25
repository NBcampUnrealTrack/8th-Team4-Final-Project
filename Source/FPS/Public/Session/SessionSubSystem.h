// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Session/RoomInfo.h"
#include "SessionSubSystem.generated.h"

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

	// 리슨 서버가 현재 방을 유지한 채 공유 로비 맵으로 돌아간다.
	// 일반 클라이언트에서는 호출할 수 없다.
	bool ReturnRoomToLobby();

	UFUNCTION(BlueprintCallable)
	void StartRoomGame(const FString& GameMapName = TEXT(""));

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
	bool SetRoomJoinable(bool bIsJoinable);
	void CompleteLeaveRoom(bool bWasSuccessful);
	void ReturnToFrontendNow();
	bool TravelRoomToLobbyNow();

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
		ReturnToFrontend
	};

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

	int32 CachedMaxPlayers = 4;
	FString CachedRoomName = TEXT("Player's Room");
	FString CachedMapName = TEXT("BombMap");
	FString CachedMatchType = TEXT("FreeForAll");
	FString CachedMapPath = TEXT("/Game/Maps/BombMap");

	UPROPERTY(EditDefaultsOnly, Category = "Session|Travel")
	FString LobbyMapPath = TEXT("/Game/Maps/Lobby");
};
