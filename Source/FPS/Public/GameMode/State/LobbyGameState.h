#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/LobbyTypes.h"
#include "GameFramework/GameStateBase.h"
#include "LobbyGameState.generated.h"

class APlayerState;

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

	// Logout 시점엔 나간 PlayerState 가 아직 PlayerArray 에 남아 있어, 실제로 빠질 때 다시 알린다
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void NotifyLobbyPlayerListChanged();

	UFUNCTION(BlueprintPure, Category = "Lobby")
	int32 GetLobbyRevision() const { return LobbyRevision; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Game Mode")
	ELobbyMatchMode GetSelectedMatchMode() const { return SelectedMatchMode; }

	// Selection authority is validated by LobbyGameMode.
	void SetSelectedMatchMode(ELobbyMatchMode NewMatchMode);

	UFUNCTION(BlueprintPure, Category = "Lobby|Room")
	const FLobbyRoomSettings& GetRoomSettings() const { return RoomSettings; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Room")
	APlayerState* GetHostPlayerState() const { return HostPlayerState; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Match Start")
	const FLobbyStartState& GetStartState() const { return StartState; }

	void SetRoomSettings(const FLobbyRoomSettings& NewRoomSettings);
	void SetHostPlayerState(APlayerState* NewHostPlayerState);

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnLobbyPlayerListChangedSignature OnLobbyPlayerListChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Game Mode")
	FOnLobbyMatchModeChangedSignature OnLobbyMatchModeChanged;

private:
	UFUNCTION()
	void OnRep_LobbyRevision();

	UFUNCTION()
	void OnRep_SelectedMatchMode();

	UFUNCTION()
	void OnRep_RoomSettings();

	UFUNCTION()
	void OnRep_HostPlayerState();

	UFUNCTION()
	void OnRep_StartState();

	UPROPERTY(ReplicatedUsing = OnRep_LobbyRevision, VisibleInstanceOnly, Category = "Lobby")
	int32 LobbyRevision = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedMatchMode, VisibleInstanceOnly, Category = "Lobby|Game Mode")
	ELobbyMatchMode SelectedMatchMode = ELobbyMatchMode::Domination;

	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings, VisibleInstanceOnly, Category = "Lobby|Room")
	FLobbyRoomSettings RoomSettings;

	UPROPERTY(ReplicatedUsing = OnRep_HostPlayerState, VisibleInstanceOnly, Category = "Lobby|Room")
	TObjectPtr<APlayerState> HostPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_StartState, VisibleInstanceOnly, Category = "Lobby|Match Start")
	FLobbyStartState StartState;
};
