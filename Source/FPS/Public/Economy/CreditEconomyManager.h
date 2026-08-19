#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "CreditEconomyManager.generated.h"

class AShooterPlayerState;
enum class ECreditChangeReason : uint8;
enum class EGameTeam : uint8;

/**
 * 매치 단위 크레딧 규칙을 실행하는 서버 전용 조정자.
 * 라운드 성과를 누적하고 종료 시 참가자 Wallet에 정산한다.
 */
UCLASS()
class FPS_API UCreditEconomyManager : public UObject
{
	GENERATED_BODY()

public:
	bool InitializePlayerForMatch(AShooterPlayerState* PlayerState);
	bool ResetForNewMatch(const TArray<AShooterPlayerState*>& PlayerStates);

	bool BeginRound(int32 RoundNumber);
	void CancelActiveRound();

	bool RecordKillReward(AShooterPlayerState* PlayerState);

	/**
	 * 목표(설치/해체)를 달성한 팀을 라운드 보상 대상으로 예약한다. 즉시 지급하지 않는다.
	 *
	 * - 지급 시점은 SettleRoundCredits이며, 대상은 해당 팀의 라운드 참가자 전원이다.
	 * - 금액은 URogueEconomySettings가 결정하므로 호출자가 주입할 수 없다.
	 * - 라운드 승패와 무관하게 유지된다. 따라서 설치 후 해체당한 라운드에서는
	 *   공격팀과 수비팀이 모두 목표 보상을 받는다. (의도된 동작)
	 * - 같은 팀에 중복 통지가 와도 라운드당 1회만 적립된다.
	 *
	 * @return 예약에 성공했거나 이미 예약된 상태면 true. 현재 정산 상태 또는 설정이 유효하지 않으면 false.
	 */
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
