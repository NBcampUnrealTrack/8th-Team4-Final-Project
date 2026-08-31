#pragma once

#include "CoreMinimal.h"
#include "GameMode/State/BaseTeamMatchGameState.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDominationFlowStateChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDominationObjectiveFlowStateChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDominationActivePointChangedSignature,
	EDominationPointId,
	ActivePointId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDominationCaptureStateChangedSignature,
	FDominationCaptureState,
	CaptureState);

UCLASS()
class FPS_API ADominationGameState : public ABaseTeamMatchGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Domination|Game Flow")
	EDominationMatchPhase GetDominationPhase() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Game Flow")
	float GetPhaseTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Round")
	int32 GetCompletedRoundCount() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Round")
	int32 GetCurrentRoundNumber() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Round")
	int32 GetRoundsToWin() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Round")
	int32 GetMaxRounds() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	EDominationPointId GetActivePointId() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	bool HasActivePoint() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	EDominationObjectivePhase GetObjectivePhase() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	bool IsObjectiveCaptureEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	bool IsPointTransitioning() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	float GetPointTransitionTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	FDominationCaptureState GetCaptureState() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	float GetCaptureProgress() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	EGameTeam GetCapturingTeam() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	bool IsCaptureContested() const;

	void SetDominationPhase(EDominationMatchPhase NewPhase, float PhaseDuration = 0.0f);
	void SetObjectiveState(
		EDominationPointId NewActivePointId,
		EDominationObjectivePhase NewPhase,
		float PhaseDuration = 0.0f);
	void SetCaptureState(const FDominationCaptureState& NewCaptureState);

	UPROPERTY(BlueprintAssignable, Category = "Domination|Game Flow")
	FOnDominationFlowStateChangedSignature OnDominationFlowStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Domination|Objective")
	FOnDominationActivePointChangedSignature OnActivePointChanged;

	UPROPERTY(BlueprintAssignable, Category = "Domination|Objective")
	FOnDominationObjectiveFlowStateChangedSignature OnObjectiveFlowStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Domination|Objective")
	FOnDominationCaptureStateChangedSignature OnCaptureStateChanged;

private:
	UFUNCTION()
	void OnRep_MatchFlowState();

	UFUNCTION()
	void OnRep_ObjectiveFlowState();

	UFUNCTION()
	void OnRep_CaptureState();

	void BroadcastMatchFlowStateChanged();

	UPROPERTY(ReplicatedUsing = OnRep_MatchFlowState, VisibleInstanceOnly, Category = "Domination|Game Flow")
	FDominationMatchFlowState MatchFlowState;

	UPROPERTY(ReplicatedUsing = OnRep_ObjectiveFlowState, VisibleInstanceOnly, Category = "Domination|Objective")
	FDominationObjectiveFlowState ObjectiveFlowState;

	UPROPERTY(ReplicatedUsing = OnRep_CaptureState, VisibleInstanceOnly, Category = "Domination|Objective")
	FDominationCaptureState CaptureState;
};
