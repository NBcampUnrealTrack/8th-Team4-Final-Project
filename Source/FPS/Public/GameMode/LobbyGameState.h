#pragma once

#include "CoreMinimal.h"
#include "GameMode/LobbyTypes.h"
#include "GameFramework/GameStateBase.h"
#include "LobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyPlayerListChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnLobbyMatchModeChangedSignature,
	ELobbyMatchMode,
	SelectedMatchMode);

UCLASS()
class FPS_API ALobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void NotifyLobbyPlayerListChanged();

	UFUNCTION(BlueprintPure, Category = "Lobby")
	int32 GetLobbyRevision() const { return LobbyRevision; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Game Mode")
	ELobbyMatchMode GetSelectedMatchMode() const { return SelectedMatchMode; }

	// Selection authority is validated by LobbyGameMode.
	void SetSelectedMatchMode(ELobbyMatchMode NewMatchMode);

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnLobbyPlayerListChangedSignature OnLobbyPlayerListChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Game Mode")
	FOnLobbyMatchModeChangedSignature OnLobbyMatchModeChanged;

private:
	UFUNCTION()
	void OnRep_LobbyRevision();

	UFUNCTION()
	void OnRep_SelectedMatchMode();

	UPROPERTY(ReplicatedUsing = OnRep_LobbyRevision, VisibleInstanceOnly, Category = "Lobby")
	int32 LobbyRevision = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedMatchMode, VisibleInstanceOnly, Category = "Lobby|Game Mode")
	ELobbyMatchMode SelectedMatchMode = ELobbyMatchMode::Bomb;
};
