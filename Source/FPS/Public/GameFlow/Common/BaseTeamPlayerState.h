#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Common/TeamTypes.h"
#include "GameFramework/PlayerState.h"
#include "BaseTeamPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerTeamChangedSignature, EGameTeam, PreviousTeam, EGameTeam, NewTeam);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerAliveStateChangedSignature, bool, bNewIsAlive);

UCLASS()
class FPS_API ABaseTeamPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ABaseTeamPlayerState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Team")
	EGameTeam GetTeam() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	bool IsAlive() const;

	void SetTeam(EGameTeam NewTeam);
	void SetAlive(bool bNewIsAlive);

protected:
	UFUNCTION()
	void OnRep_Team(EGameTeam PreviousTeam);

	UFUNCTION()
	void OnRep_IsAlive();

private:
	UPROPERTY(ReplicatedUsing = OnRep_Team, VisibleInstanceOnly, Category = "Game Flow|Team")
	EGameTeam Team = EGameTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_IsAlive, VisibleInstanceOnly, Category = "Game Flow|Round")
	bool bIsAlive = false;

public:
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Team")
	FOnPlayerTeamChangedSignature OnPlayerTeamChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Round")
	FOnPlayerAliveStateChangedSignature OnPlayerAliveStateChanged;
};
