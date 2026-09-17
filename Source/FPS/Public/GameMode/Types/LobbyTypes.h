#pragma once

#include "CoreMinimal.h"
#include "LobbyTypes.generated.h"

UENUM(BlueprintType)
enum class ELobbyMatchMode : uint8
{
	Bomb UMETA(DisplayName = "Bomb"),
	Domination UMETA(DisplayName = "Domination")
};

// 시작을 막고 있는 사유
UENUM(BlueprintType)
enum class ELobbyStartBlocker : uint8
{
	None,
	Starting,                  // 시작 요청 처리 중이거나 게임 맵으로 이동 중
	NotEnoughPlayers,
	TeamsCannotBeFinalized,
	WaitingForReady
};

USTRUCT(BlueprintType)
struct FLobbyStartState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	ELobbyStartBlocker Blocker = ELobbyStartBlocker::NotEnoughPlayers;
	// 호스트를 뺀 레디 수 / 호스트를 뺀 인원
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 ReadyCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 ReadyRequired = 0;
};

USTRUCT(BlueprintType)
struct FLobbyRoomSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FString RoomName;
	// 방 수용 인원 
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 MaxPlayers = 0;
	// 팀 정원 (게임 규칙 4V4 )
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 MaxPlayersPerTeam = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 MinimumPlayersToStart = 0;
};
