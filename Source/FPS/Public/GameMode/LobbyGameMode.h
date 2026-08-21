#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LobbyGameMode.generated.h"

class APlayerState;

UCLASS()
class FPS_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALobbyGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	bool RequestStartRoomGame(APlayerController* RequestingPlayer, const FString& GameMapName = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool CanStartRoomGame() const;

private:
	void AssignHostPlayerIfNeeded(APlayerController* NewPlayer);
	void ReassignHostPlayerIfNeeded(const APlayerState* ExitingPlayerState);
	void NotifyLobbyPlayerListChanged();
	bool AreAllClientsReady() const;
	bool IsHostPlayerState(const APlayerState* PlayerState) const;
	FString ResolveMapPathFromName(const FString& MapName) const;
	void TravelToPendingGameMap();

	UFUNCTION()
	void HandleStartRoomGameComplete(bool bWasSuccessful);

private:
	TWeakObjectPtr<APlayerState> HostPlayerState;
	FString PendingGameMapPath = TEXT("/Game/Maps/BombMap");
	bool bStartRoomGamePending = false;
	bool bTravelingToGameMap = false;
};
