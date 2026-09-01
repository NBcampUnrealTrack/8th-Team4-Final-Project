#pragma once

#include "CoreMinimal.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "BaseTeamMatchGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchStateChangedSignature);

// 팀 기반 매치가 공통으로 사용하는 최종 승자 상태를 소유한다.
UCLASS(Abstract)
class FPS_API ABaseTeamMatchGameState : public ABaseTeamGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Match")
	EGameTeam GetMatchWinner() const;

	void SetMatchWinner(EGameTeam WinningTeam);

	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Match")
	FOnMatchStateChangedSignature OnMatchStateChanged;

protected:
	// 파생 GameState가 기존 모드별 상태 변경 알림을 함께 유지할 수 있는 확장 지점이다.
	virtual void HandleMatchStateChanged();

private:
	UFUNCTION()
	void OnRep_MatchWinner();

	void BroadcastMatchStateChanged();

	UPROPERTY(ReplicatedUsing = OnRep_MatchWinner, VisibleInstanceOnly, Category = "Game Flow|Match")
	EGameTeam MatchWinner = EGameTeam::None;
};
