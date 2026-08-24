#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "CreditEconomyManager.generated.h"

class AShooterPlayerState;
enum class ECreditChangeReason : uint8;
enum class EGameTeam : uint8;


// 매치 단위 크레딧 규칙을 관리한다.
// 라운드 보상을 누적하고 종료 시 참가자의 Wallet에 정산한다.

UCLASS()
class FPS_API UCreditEconomyManager : public UObject
{
	GENERATED_BODY()

public:
	bool InitializePlayerForMatch(AShooterPlayerState* PlayerState);

	// 초기화가 지금 가능한지만 확인한다. 상태는 바꾸지 않는다.
	bool CanResetForNewMatch(
		const TArray<AShooterPlayerState*>& PlayerStates,
		FString* OutFailureReason = nullptr) const;

	bool ResetForNewMatch(const TArray<AShooterPlayerState*>& PlayerStates);

	bool BeginRound(int32 RoundNumber);
	void CancelActiveRound();

	bool RecordKillReward(AShooterPlayerState* PlayerState);

	// 목표 달성 팀을 기록하고 실제 보상은 라운드 정산 시 지급한다.
	bool RecordTeamObjectiveReward(EGameTeam Team);

	bool SettleRoundCredits(
		int32 RoundNumber,
		EGameTeam WinningTeam,
		const TArray<AShooterPlayerState*>& RoundParticipants);

	bool IsConfigurationValid() const;

private:
	bool AddPendingKillReward(AShooterPlayerState* PlayerState, int32 Amount);

	bool ResetPlayerCredits(
		AShooterPlayerState* PlayerState,
		int32 StartingCredits,
		ECreditChangeReason Reason);

private:
	TSet<TWeakObjectPtr<AShooterPlayerState>> InitializedPlayersThisMatch;
	TMap<TWeakObjectPtr<AShooterPlayerState>, int32> PendingKillRewards;
	TSet<EGameTeam> ObjectiveRewardedTeamsThisRound;

	int32 ActiveRoundNumber = 0;
	int32 LastSettledRoundNumber = 0;

	bool bIsProcessingEconomyUpdate = false;
};