#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Common/TeamTypes.h"
#include "UObject/Object.h"
#include "TeamPlayerManager.generated.h"

class AActor;
class ABaseTeamGameMode;
class ABaseTeamPlayerState;
class AController;
class APlayerController;
class AShooterPlayerState;

struct FTeamRoundParticipant
{
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<AShooterPlayerState> PlayerState;
};

/**
 * 서버 전용 팀/플레이어 수명주기 관리자.
 * 라운드 스폰과 참가자 스냅샷을 한 경계에서 관리한다.
 */
UCLASS()
class FPS_API UTeamPlayerManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ABaseTeamGameMode* InOwnerGameMode, float InSpawnClearanceRadius);

	int32 GetTeamPlayerCount(EGameTeam Team) const;
	int32 GetAlivePlayerCount(EGameTeam Team) const;
	bool AssignTeamIfNeeded(APlayerController* PlayerController, EGameTeam TeamToAssign) const;

	AActor* SelectAvailablePlayerStart(const TArray<AActor*>& MatchingStarts, const TCHAR* StartDescription) const;

	void RemovePlayerPawn(APlayerController* PlayerController) const;
	void ResetAllPlayerPawns();
	bool PreparePlayersForRound();

	bool AreRoundParticipantsReady() const;
	void GetRoundParticipants(TArray<AShooterPlayerState*>& OutParticipants) const;
	void GetRoundParticipantControllers(TArray<APlayerController*>& OutControllers) const;

	bool TryMarkPlayerEliminated(AController* EliminatedController, ABaseTeamPlayerState*& OutPlayerState);

private:
	bool IsRoundParticipant(const ABaseTeamPlayerState* PlayerState) const;
	void ResetRoundTracking();
	bool GatherEligiblePlayers(TArray<APlayerController*>& OutPlayers) const;
	void CleanupFailedPreparation(const TArray<APlayerController*>& AttemptedPlayers);
	bool IsPlayerStartClear(const AActor* PlayerStart) const;
	ABaseTeamGameMode* GetOwnerGameMode() const;

	TWeakObjectPtr<ABaseTeamGameMode> OwnerGameMode;
	TArray<FTeamRoundParticipant> RoundParticipants;
	TSet<TWeakObjectPtr<ABaseTeamPlayerState>> EliminatedPlayersThisRound;
	float SpawnClearanceRadius = 0.0f;
};
