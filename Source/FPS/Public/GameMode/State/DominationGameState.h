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
	FDominationObjectiveFlowState GetObjectiveFlowState() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	EDominationPointId GetLastCapturedPointId() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	EGameTeam GetLastCapturingTeam() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	int32 GetObjectiveCaptureSerial() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	FDominationCaptureState GetCaptureState() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	float GetCaptureProgress() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	EGameTeam GetCapturingTeam() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	bool IsCaptureContested() const;

	void SetDominationPhase(EDominationMatchPhase NewPhase, float PhaseDuration = 0.0f);
	// ObjectiveManager가 의미 단위로 상태를 게시하도록 저수준 필드 조합을 외부에 노출하지 않는다.
	void ResetObjectiveState();
	void BeginInitialObjectiveTransition(
		EDominationPointId InitialPointId,
		float PhaseDuration);
	void BeginObjectiveTransitionAfterCapture(
		EDominationPointId CapturedPointId,
		EGameTeam CapturingTeam,
		EDominationPointId NextPointId,
		float PhaseDuration);
	void RecordObjectiveCapture(
		EDominationPointId CapturedPointId,
		EGameTeam CapturingTeam);
	void SetObjectiveCaptureEnabled(EDominationPointId ActivePointId);
	void SetObjectiveInactive(EDominationPointId ActivePointId);
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
	void ApplyObjectiveState(
		EDominationPointId NewActivePointId,
		EDominationObjectivePhase NewPhase,
		float PhaseDuration,
		EDominationPointId LastCapturedPointId,
		EGameTeam LastCapturingTeam,
		bool bIncrementCaptureSerial);

	UPROPERTY(ReplicatedUsing = OnRep_MatchFlowState, VisibleInstanceOnly, Category = "Domination|Game Flow")
	FDominationMatchFlowState MatchFlowState;

	UPROPERTY(ReplicatedUsing = OnRep_ObjectiveFlowState, VisibleInstanceOnly, Category = "Domination|Objective")
	FDominationObjectiveFlowState ObjectiveFlowState;

	UPROPERTY(ReplicatedUsing = OnRep_CaptureState, VisibleInstanceOnly, Category = "Domination|Objective")
	FDominationCaptureState CaptureState;
};
