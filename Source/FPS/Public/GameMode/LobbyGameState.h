#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyPlayerListChangedSignature);

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

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnLobbyPlayerListChangedSignature OnLobbyPlayerListChanged;

private:
	UFUNCTION()
	void OnRep_LobbyRevision();

	UPROPERTY(ReplicatedUsing = OnRep_LobbyRevision, VisibleInstanceOnly, Category = "Lobby")
	int32 LobbyRevision = 0;
};
