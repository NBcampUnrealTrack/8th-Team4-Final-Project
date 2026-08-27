#pragma once

#include "CoreMinimal.h"
#include "GameMode/BaseTeamMatchGameState.h"
#include "GameMode/DominationTypes.h"
#include "DominationGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDominationFlowStateChangedSignature);

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

	void SetDominationPhase(EDominationMatchPhase NewPhase, float PhaseDuration = 0.0f);

	UPROPERTY(BlueprintAssignable, Category = "Domination|Game Flow")
	FOnDominationFlowStateChangedSignature OnDominationFlowStateChanged;

private:
	UFUNCTION()
	void OnRep_MatchFlowState();

	void BroadcastMatchFlowStateChanged();

	UPROPERTY(ReplicatedUsing = OnRep_MatchFlowState, VisibleInstanceOnly, Category = "Domination|Game Flow")
	FDominationMatchFlowState MatchFlowState;
};
