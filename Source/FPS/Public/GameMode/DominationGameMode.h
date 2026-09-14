#pragma once

#include "CoreMinimal.h"
#include "GameMode/BaseTeamGameMode.h"
#include "GameMode/Types/DominationTypes.h"
#include "TimerManager.h"
#include "DominationGameMode.generated.h"

class APlayerController;
class AShooterPlayerState;
class ADominationPlayerController;
class ADominationPlayerState;
class UAugmentDraftManager;
class UAugmentDraftProfile;
class UDominationLoadoutManager;
class UDominationMatchStartManager;
class UDominationObjectiveManager;
class UDominationRespawnManager;

DECLARE_MULTICAST_DELEGATE(FOnDominationMatchStarted);

DECLARE_MULTICAST_DELEGATE_FourParams(
	FOnDominationRoundCompleted,
	EDominationPointId,
	EGameTeam,
	int32,
	bool);

/**
 * Domination의 점령 라운드는 점수 단위이며, 점령마다 Pawn과 라운드 자원을
 * 일괄 재생성하는 수명주기 경계가 아니다.
 * ABaseRoundGameMode의 라운드 재준비 모델과 수명주기가 다르므로
 * 의도적으로 ABaseTeamGameMode에서 직접 파생한다.
 */
UCLASS()
class FPS_API ADominationGameMode : public ABaseTeamGameMode
{
	GENERATED_BODY()

public:
	ADominationGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Logout(AController* Exiting) override;
	virtual void NotifyPlayerEliminatedBy(
		AController* EliminatedController,
		AShooterPlayerState* KillerPlayerState) override;
	virtual void NotifyPlayerReadyForGameFlow(APlayerController* PlayerController) override;

	void RequestCombatClass(
		ADominationPlayerController* PlayerController,
		EDominationCombatClass RequestedClass);
	void RequestPreviewCombatClass(
		ADominationPlayerController* PlayerController,
		EDominationCombatClass RequestedClass);
	UDominationLoadoutManager* GetLoadoutManager() const;
	void GetInitialParticipantPlayerStates(
		TArray<ADominationPlayerState*>& OutPlayerStates) const;
	bool RestartPlayerAtObjective(
		APlayerController* PlayerController,
		EDominationPointId ObjectivePointId);
	void RespawnPlayer(AController* Controller);

	// 초기 참가자 스폰이 끝나고 중앙 거점 활성화 대기가 시작된 뒤 발생한다.
	// 외부 증강 시스템은 Manager 내부가 아니라 이 GameMode 경계에 연결한다.
	FOnDominationMatchStarted OnDominationMatchStarted;

	// 인자: 점령 거점, 라운드 승리 팀, 해당 팀 누적 점수, 매치 승리 여부.
	// 외부 보상·증강 시스템은 점수 반영이 끝난 이 이벤트에 연결한다.
	FOnDominationRoundCompleted OnRoundCompleted;

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual EPlayerParticipationState ResolveParticipationStateForNewPlayer(APlayerController* NewPlayer) const override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual bool IsSurrenderVoteAllowedNow() const override;
	virtual void HandleSurrenderApproved(EGameTeam SurrenderingTeam) override;

private:
	bool HasValidCombatClassConfiguration() const;
	bool AreAugmentDraftDependenciesReady() const;
	void HandleAugmentDraftDependenciesReady();
	void GrantAugmentRewardsForRound(int32 RewardRoundNumber);
	void ServeNextAugmentReward(AShooterPlayerState* PlayerState);
	void HandleInitialPlayersSpawned();
	void HandleAugmentDraftQueueDrained(AShooterPlayerState* PlayerState);
	void TryStartInitialObjectiveTransition();
	void HandleCaptureCompleted(
		EDominationPointId CapturedPointId,
		EGameTeam CapturingTeam);
	void FinishDominationMatch(EGameTeam WinningTeam);
	void ReturnToLobbyAfterMatch();
	bool TryJoinPlayerInProgress(
		ADominationPlayerController* PlayerController,
		EDominationCombatClass RequestedClass);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Match Start", meta = (AllowPrivateAccess = "true", ClampMin = "2", ClampMax = "8"))
	int32 MinimumPlayersToStart = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Match Start", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "4"))
	int32 MinimumPlayersPerTeam = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Match Start", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float ClassSelectionDuration = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true"))
	FDominationCaptureRules CaptureRules;

	// 최초 C 거점과 이후 전선 이동 모두 이 시간 동안 Repositioning 상태를 유지한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float PointTransitionDuration = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Respawn", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", Units = "s"))
	float RespawnDelay = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Respawn", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float RespawnProtectionDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Match End", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "s"))
	float MatchEndDelay = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Augment", meta = (AllowPrivateAccess = "true"))
	bool bEnableAugmentDraft = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Augment", meta = (AllowPrivateAccess = "true", EditCondition = "bEnableAugmentDraft"))
	TObjectPtr<UAugmentDraftProfile> AugmentDraftProfile;

	UPROPERTY(Transient)
	TObjectPtr<UDominationMatchStartManager> MatchStartManager;

	UPROPERTY(Transient)
	TObjectPtr<UDominationLoadoutManager> LoadoutManager;

	UPROPERTY(Transient)
	TObjectPtr<UDominationObjectiveManager> ObjectiveManager;

	UPROPERTY(Transient)
	TObjectPtr<UDominationRespawnManager> RespawnManager;

	UPROPERTY(Transient)
	TObjectPtr<UAugmentDraftManager> AugmentDraftManager;

	// 최초 증강을 모두 끝내야 같은 시점부터 중앙 거점 전환 시간을 받는다.
	TSet<TWeakObjectPtr<AShooterPlayerState>> PendingInitialAugmentPlayers;
	bool bInitialObjectiveTransitionStarted = false;

	FTimerHandle MatchEndTimerHandle;
};
