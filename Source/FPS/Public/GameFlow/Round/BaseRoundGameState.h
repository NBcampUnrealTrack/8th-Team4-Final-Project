#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Common/BaseTeamGameState.h"
#include "GameFlow/Round/RoundTypes.h"
#include "BaseRoundGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoundStateChangedSignature);

UCLASS()
class FPS_API ABaseRoundGameState : public ABaseTeamGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	int32 GetCurrentRoundNumber() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	EGameFlowPhase GetGameFlowPhase() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	EGameTeam GetRoundWinner() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	ERoundEndReason GetRoundEndReason() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Match")
	EGameTeam GetMatchWinner() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	ETeamRole GetTeamRole(EGameTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	EGameTeam GetTeamForRole(ETeamRole TeamRole) const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	float GetRoundTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	bool IsRoundTimerFrozen() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	float GetPhaseTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Match")
	int32 GetMaxRounds() const;

	void InitializeRoundConfiguration(int32 NewMaxRounds);
	void SetCurrentRoundNumber(int32 NewRoundNumber);
	void SetGameFlowPhase(EGameFlowPhase NewPhase, float PhaseDuration = 0.0f);
	void SetRoundTimer(float Duration);
	void FreezeRoundTimer();
	void ClearRoundTimer();
	void SetRoundResult(EGameTeam WinningTeam, ERoundEndReason EndReason);
	void ClearRoundResult();
	void SetMatchWinner(EGameTeam WinningTeam);
	void SetTeamRoles(ETeamRole NewTeamARole, ETeamRole NewTeamBRole);

protected:
	UFUNCTION()
	void OnRep_RoundState();

private:
	void BroadcastRoundStateChanged();

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Round")
	int32 CurrentRoundNumber = 0;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Round")
	EGameFlowPhase GameFlowPhase = EGameFlowPhase::WaitingForPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Round")
	EGameTeam RoundWinner = EGameTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Round")
	ERoundEndReason RoundEndReason = ERoundEndReason::None;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Match")
	EGameTeam MatchWinner = EGameTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Round")
	ETeamRole TeamARole = ETeamRole::Attackers;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Round")
	ETeamRole TeamBRole = ETeamRole::Defenders;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Round")
	float RoundEndServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Round")
	float FrozenRoundTimeRemaining = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Round")
	bool bRoundTimerFrozen = false;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Round")
	float PhaseEndServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, VisibleInstanceOnly, Category = "Game Flow|Match")
	int32 MaxRounds = 0;

public:
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Round")
	FOnRoundStateChangedSignature OnRoundStateChanged;
};
