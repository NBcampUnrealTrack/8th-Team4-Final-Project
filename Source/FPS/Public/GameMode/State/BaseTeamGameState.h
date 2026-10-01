#pragma once

#include "CoreMinimal.h"
#include "Chat/ChatTypes.h"
#include "GameMode/Types/TeamTypes.h"
#include "GameFramework/GameState.h"
#include "GameplayTagContainer.h"
#include "Templates/Function.h"
#include "BaseTeamGameState.generated.h"

class AShooterPlayerController;
class AShooterPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamScoreChangedSignature, EGameTeam, Team, int32, NewScore);

// 킬로그에 필요한 최소 정보
USTRUCT(BlueprintType)
struct FShooterEliminationEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Elimination")
	TObjectPtr<AShooterPlayerState> KillerPlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Elimination")
	TObjectPtr<AShooterPlayerState> VictimPlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Elimination")
	FGameplayTag WeaponTag;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Elimination")
	bool bHeadshot = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerEliminatedSignature, const FShooterEliminationEvent&, EliminationEvent);

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

	// 서버 전용. 모든 머신의 OnPlayerEliminated로 이어진다.
	void BroadcastPlayerEliminated(const FShooterEliminationEvent& EliminationEvent);

protected:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayerEliminated(const FShooterEliminationEvent& EliminationEvent);

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

	// 모든 머신에서 발신, 킬로그가 구독
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Elimination")
	FOnPlayerEliminatedSignature OnPlayerEliminated;
};
