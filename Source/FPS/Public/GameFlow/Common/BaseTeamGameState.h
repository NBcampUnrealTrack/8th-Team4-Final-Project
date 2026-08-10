#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Common/TeamTypes.h"
#include "GameFramework/GameState.h"
#include "BaseTeamGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamScoreChangedSignature, EGameTeam, Team, int32, NewScore);

// Stores mode-agnostic team scores and exposes them to gameplay UI.
UCLASS()
class FPS_API ABaseTeamGameState : public AGameState
{
	GENERATED_BODY()

public:
	ABaseTeamGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	int32 GetTeamScore(EGameTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	int32 GetTeamAScore() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	int32 GetTeamBScore() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	EGameTeam GetLeadingTeam() const;

	void AddTeamScore(EGameTeam Team, int32 ScoreAmount = 1);
	void ResetTeamScores();

protected:
	UFUNCTION()
	void OnRep_TeamAScore();

	UFUNCTION()
	void OnRep_TeamBScore();

private:
	UPROPERTY(ReplicatedUsing = OnRep_TeamAScore, VisibleInstanceOnly, Category = "Game Flow|Score")
	int32 TeamAScore = 0;

	UPROPERTY(ReplicatedUsing = OnRep_TeamBScore, VisibleInstanceOnly, Category = "Game Flow|Score")
	int32 TeamBScore = 0;

public:
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Score")
	FOnTeamScoreChangedSignature OnTeamScoreChanged;
};
