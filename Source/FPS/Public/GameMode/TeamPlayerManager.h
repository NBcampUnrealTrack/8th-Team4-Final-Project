#pragma once

#include "CoreMinimal.h"
#include "GameMode/TeamTypes.h"
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
 * GameMode가 모드별 PlayerStart를 역할/팀 단위로 모아 전달하는 일반화된 요청이다.
 * TeamPlayerManager는 역할의 의미를 모르고 개수, 간격, 현재 점유 여부만 검사한다.
 */
struct FPlayerStartGroup
{
	TArray<AActor*> PlayerStarts;
	FString Description;
	int32 RequiredCount = 0;
};

enum class EPlayerStartReservationResult : uint8
{
	Succeeded,
	InvalidConfiguration,
	TemporarilyBlocked
};

enum class ERoundPlayerPreparationResult : uint8
{
	Succeeded,
	InvalidState,
	TemporarilyBlocked
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

	EPlayerStartReservationResult ReservePlayerStarts(const TArray<FPlayerStartGroup>& StartGroups);
	void ClearPlayerStartReservations();
	AActor* SelectAvailablePlayerStart(const TArray<AActor*>& MatchingStarts, const TCHAR* StartDescription);

	void RemovePlayerPawn(APlayerController* PlayerController) const;
	void ResetAllPlayerPawns();
	ERoundPlayerPreparationResult PreparePlayersForRound();

	bool AreRoundParticipantsReady() const;
	void GetRoundParticipants(TArray<AShooterPlayerState*>& OutParticipants) const;
	void GetRoundParticipantControllers(TArray<APlayerController*>& OutControllers) const;

	bool TryMarkPlayerEliminated(AController* EliminatedController, ABaseTeamPlayerState*& OutPlayerState);

private:
	bool IsRoundParticipant(const ABaseTeamPlayerState* PlayerState) const;
	void ResetRoundTracking();
	bool GatherEligiblePlayers(TArray<APlayerController*>& OutPlayers) const;
	void CleanupFailedPreparation(const TArray<APlayerController*>& AttemptedPlayers);
	bool TryBuildStartReservation(
		const TArray<FPlayerStartGroup>& StartGroups,
		int32 GroupIndex,
		int32 CandidateIndex,
		int32 RemainingInGroup,
		bool bRequireClearStarts,
		TArray<AActor*>& InOutSelectedStarts) const;
	bool IsCompatibleWithSelectedStarts(const AActor* Candidate, const TArray<AActor*>& SelectedStarts) const;
	bool IsPlayerStartClear(const AActor* PlayerStart) const;
	ABaseTeamGameMode* GetOwnerGameMode() const;

	TWeakObjectPtr<ABaseTeamGameMode> OwnerGameMode;
	TArray<FTeamRoundParticipant> RoundParticipants;
	TSet<TWeakObjectPtr<ABaseTeamPlayerState>> EliminatedPlayersThisRound;
	TSet<TWeakObjectPtr<AActor>> ReservedPlayerStarts;
	float SpawnClearanceRadius = 0.0f;
	bool bHasActivePlayerStartReservation = false;
};
