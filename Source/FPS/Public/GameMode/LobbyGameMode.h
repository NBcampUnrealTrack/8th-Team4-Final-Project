#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/LobbyTypes.h"
#include "GameMode/Types/TeamTypes.h"
#include "GameFramework/GameModeBase.h"
#include "LobbyGameMode.generated.h"

class AShooterPlayerState;
class APlayerState;

UCLASS()
class FPS_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALobbyGameMode();

	virtual void InitGameState() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void PostSeamlessTravel() override;
	virtual void Logout(AController* Exiting) override;

	// GameMapName remains for existing Blueprint/C++ callers.
	// When empty, the replicated lobby selection is used.
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	bool RequestStartRoomGame(APlayerController* RequestingPlayer, const FString& GameMapName = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool CanStartRoomGame() const;

	// 방 화면이 읽을 시작 가능 여부
	FLobbyStartState BuildStartState() const;

	UFUNCTION(BlueprintCallable, Category = "Lobby|Game Mode")
	bool RequestLobbyMatchMode(APlayerController* RequestingPlayer, ELobbyMatchMode RequestedMatchMode);

	UFUNCTION(BlueprintPure, Category = "Lobby|Game Mode")
	ELobbyMatchMode GetSelectedMatchMode() const;

	UFUNCTION(BlueprintPure, Category = "Lobby|Game Mode")
	FString GetSelectedGameMapName() const;

	// Allows players to select a team. Requests for a full team are rejected.
	UFUNCTION(BlueprintCallable, Category = "Lobby|Team")
	bool RequestLobbyTeam(APlayerController* RequestingPlayer, EGameTeam RequestedTeam);

	UFUNCTION(BlueprintPure, Category = "Lobby|Team")
	int32 CountTeamPlayers(EGameTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Lobby|Team")
	int32 GetMaxPlayersPerTeam() const { return MaxPlayersPerTeam; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Match Start", meta = (ClampMin = "2", ClampMax = "8"))
	int32 MinimumPlayersToStart = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Match Start", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MinimumPlayersPerTeam = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Team", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxPlayersPerTeam = 4;

	// Short map names are resolved below /Game/Maps. Full /Game/... paths are also supported.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Game Mode")
	FString BombMapName = TEXT("BombMap");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Game Mode")
	FString DominationMapName = TEXT("DominationMap");

private:
	void AssignHostPlayerIfNeeded(APlayerController* NewPlayer);
	void RebuildLobbyStateAfterSeamlessTravel();
	void ReassignHostPlayerIfNeeded(const APlayerState* ExitingPlayerState);
	void NotifyLobbyPlayerListChanged();
	bool AreAllClientsReady() const;
	// 로그 없는 레디 집계
	void CountClientReady(int32& OutReady, int32& OutTotal) const;
	bool HasMinimumPlayersToStart() const;
	bool CanFinalizeTeamsForMatchStart() const;
	bool IsHostPlayerState(const APlayerState* PlayerState) const;
	bool IsSupportedMatchMode(ELobbyMatchMode MatchMode) const;
	bool TryResolveMatchModeFromMapName(const FString& MapName, ELobbyMatchMode& OutMatchMode) const;
	void GetTeamAssignmentCounts(int32& OutTeamA, int32& OutTeamB, int32& OutUnassigned) const;
	FString GetMapNameForMatchMode(ELobbyMatchMode MatchMode) const;
	FString GetMatchTypeForMatchMode(ELobbyMatchMode MatchMode) const;
	bool UpdateSessionAdvertisementForMatchMode(ELobbyMatchMode MatchMode);
	FString ResolveMapPathFromName(const FString& MapName) const;
	void TravelToPendingGameMap();

	// Only unassigned players are auto-filled. Explicit team choices are preserved.
	bool FinalizeTeamsForMatchStart();

	UFUNCTION()
	void HandleStartRoomGameComplete(bool bWasSuccessful);

private:
	TWeakObjectPtr<APlayerState> HostPlayerState;
	FString PendingGameMapPath = TEXT("/Game/Maps/BombMap");
	bool bStartRoomGamePending = false;
	bool bTravelingToGameMap = false;
};
