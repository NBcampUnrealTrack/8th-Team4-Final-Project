#pragma once

#include "CoreMinimal.h"
#include "GameMode/TeamTypes.h"
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

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void PostSeamlessTravel() override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	bool RequestStartRoomGame(APlayerController* RequestingPlayer, const FString& GameMapName = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool CanStartRoomGame() const;

	// 로비에서 플레이어가 직접 팀을 고를 수 있도록 추가. 정원이 찬 팀은 거부
	UFUNCTION(BlueprintCallable, Category = "Lobby|Team")
	bool RequestLobbyTeam(APlayerController* RequestingPlayer, EGameTeam RequestedTeam);

	UFUNCTION(BlueprintPure, Category = "Lobby|Team")
	int32 CountTeamPlayers(EGameTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Lobby|Team")
	int32 GetMaxPlayersPerTeam() const { return MaxPlayersPerTeam; }

protected:
	// 팀당 정원
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Team", meta = (ClampMin = "1"))
	int32 MaxPlayersPerTeam = 4;

private:
	void AssignHostPlayerIfNeeded(APlayerController* NewPlayer);
	void RebuildLobbyStateAfterSeamlessTravel();
	void ReassignHostPlayerIfNeeded(const APlayerState* ExitingPlayerState);
	void NotifyLobbyPlayerListChanged();
	bool AreAllClientsReady() const;
	bool IsHostPlayerState(const APlayerState* PlayerState) const;
	FString ResolveMapPathFromName(const FString& MapName) const;
	void TravelToPendingGameMap();

	// 미선택 플레이어 무작위 배정 + 양 팀 인원 차이를 1명 이하로 조정
	void BalanceTeamsForMatchStart();

	UFUNCTION()
	void HandleStartRoomGameComplete(bool bWasSuccessful);

private:
	TWeakObjectPtr<APlayerState> HostPlayerState;
	FString PendingGameMapPath = TEXT("/Game/Maps/BombMap");
	bool bStartRoomGamePending = false;
	bool bTravelingToGameMap = false;
};
