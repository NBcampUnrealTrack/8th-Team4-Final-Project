#pragma once

#include "CoreMinimal.h"
#include "GameMode/BaseTeamGameMode.h"
#include "GameMode/InitialPlayerReadinessTracker.h"
#include "GameMode/RoundTypes.h"
#include "BaseRoundGameMode.generated.h"

class ABaseRoundGameState;
class ABaseTeamPlayerState;
class APlayerController;
class AShooterPlayerState;
class UAugmentDraftManager;
class UCreditEconomyManager;
class UShopPurchaseComponent;
enum class EPlayerStartReservationResult : uint8;

/**
 * 서버 권위 라운드 흐름 조정자.
 * 페이즈와 승패만 결정하고, 플레이어 및 드래프트 작업은 각 Manager에 맡긴다.
 */
UCLASS(Abstract)
class FPS_API ABaseRoundGameMode : public ABaseTeamGameMode
{
	GENERATED_BODY()

public:
	ABaseRoundGameMode();
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game Flow|Round")
	void NotifyPlayerEliminated(AController* EliminatedController);

	// DeathContext가 있는 C++ 사망 경로에서 킬러 정보까지 전달한다.
	virtual void NotifyPlayerEliminatedBy(
		AController* EliminatedController,
		AShooterPlayerState* KillerPlayerState) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game Flow|Match")
	void RestartGameFlow();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game Flow|Round")
	void RetryRoundOpening();

	// 각 로컬 플레이어의 인게임 UI가 준비되면 첫 라운드 시작 조건을 다시 평가한다.
	virtual void NotifyPlayerReadyForGameFlow(APlayerController* PlayerController) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitGameState() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual bool ReadyToEndMatch_Implementation() override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;
	virtual EPlayerParticipationState ResolveParticipationStateForNewPlayer(APlayerController* NewPlayer) const override;

	// 파생 모드는 목표물 수명주기와 승패 규칙만 구현한다.
	virtual void ResetModeState();
	virtual EPlayerStartReservationResult ReserveModePlayerStarts(const FRoundContext& RoundContext);
	virtual bool PrepareModeRound(const FRoundContext& RoundContext);
	virtual bool ActivateModeRound();
	virtual void DeactivateModeRound(EGameTeam WinningTeam, ERoundEndReason EndReason);
	virtual void HandleModePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState);
	virtual void ResolveModeRoundTimeExpired();
	virtual EGameTeam ResolveModeMatchWinnerAfterPlayerDeparture(EGameTeam DepartingTeam) const;
	virtual bool HasModeRequiredPlayers() const;
	virtual bool IsModeConfigurationValid() const;
	virtual bool IsSurrenderVoteAllowedNow() const override;
	virtual void HandleSurrenderApproved(EGameTeam SurrenderingTeam) override;

	void EndRound(EGameTeam WinningTeam, ERoundEndReason EndReason);
	void FreezeRoundTimer();

	/**
	 * 목표를 달성한 역할의 팀에 라운드 보상을 예약한다. 지급은 라운드 정산 시점이다.
	 * 파생 모드가 크레딧에 접근하는 유일한 통로이며, 실패해도 게임 규칙 진행은 중단하지 않는다.
	 */
	void RecordRoleObjectiveReward(ETeamRole ObjectiveRole);

	ABaseRoundGameState* GetBaseRoundGameState() const;
	ETeamRole GetRoleForTeam(EGameTeam Team) const;
	EGameTeam GetTeamForRole(ETeamRole TeamRole) const;
	ETeamRole GetSpawnRoleForTeam(EGameTeam Team) const;

private:
	void InitializeGameFlow();
	void HandlePlayerRosterChanged();
	bool TryFinishMatchForPlayerDeparture(EGameTeam DepartingTeam);
	void HandleDraftDependenciesReady();
	bool InitializeRoundParticipantCredits();
	bool ResetEconomyForNewMatch();
	void SettleCompletedRoundCredits(int32 RoundNumber, EGameTeam WinningTeam);
	bool GatherRoundParticipantPurchaseComponents(
		TArray<UShopPurchaseComponent*>& OutPurchaseComponents) const;
	bool CanCommitPendingShopPurchases() const;
	bool CommitPendingShopPurchases();

	// 개별 플레이어의 미확정 거래를 같은 정책과 로그 형식으로 정리한다.
	void RefundPendingShopPurchasesForPlayer(
		AShooterPlayerState* PlayerState,
		const TCHAR* Context);

	// 흐름이 되감기거나 끝날 때 모든 플레이어의 미확정 상점 결제를 되돌린다.
	// 거래가 자기가 열린 페이즈를 넘기지 않도록 라운드/매치 경계에서 호출한다.
	void RefundPendingShopPurchases(const TCHAR* Context);

	// 상위 흐름은 아래 전이만 따라가면 된다.
	void TryBeginNextRound();       // Waiting/Ready -> RoundStarting
	void OpenNextRound();           // RoundStarting -> Draft 또는 Combat
	void FinishAugmentDraft();      // Draft -> Combat
	bool TryStartRoundCombat();     // 준비 완료 -> RoundInProgress
	void AdvanceToNextRound();      // RoundEnding -> Waiting
	void FinishMatch(EGameTeam WinningTeam);
	void ReturnToLobbyAfterMatch();

	void EnterWaitingForPlayers();
	void EnterWaitingForPlayersReady();
	void HandleInitialReadyTimeout();
	bool IsInitialJoinPhase(EGameFlowPhase Phase) const;
	void ScheduleSpawnPreparationRetry(const TCHAR* Reason);
	void SuspendRoundOpening(const TCHAR* Reason);
	FRoundContext MakeRoundContext(int32 RoundNumber) const;

	bool HasRequiredPlayersToStart() const;
	bool AreInitialPlayersReadyForGameFlow() const;
	void UpdateInitialLoadingStatus();
	bool IsGameFlowConfigurationValid() const;
	bool IsBaseConfigurationValid() const;
	bool IsMatchComplete(EGameTeam& OutWinningTeam) const;

	void StartRoundTimer(float Duration);
	void HandleRoundTimerExpired();
	void ClearPhaseTimer();
	void ClearRoundTimer();
	void ClearInitialReadyTimer();
	void ClearAllFlowTimers();

	void SetAllPlayersRoundInputEnabled(bool bEnabled);
	void SetRoundParticipantInputEnabled(bool bEnabled);
	void SetPlayerRoundInputEnabled(APlayerController* PlayerController, bool bEnabled) const;
	UAugmentDraftManager* GetAugmentDraftManager() const;
	UCreditEconomyManager* GetCreditEconomyManager() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Players", meta = (ClampMin = "1"))
	int32 MinimumPlayersToStart = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Timing", meta = (ClampMin = "0.0"))
	float InitialRoundStartDelay = 5.0f;

	// 첫 라운드에서 모든 참가자의 인게임 UI 준비 신호를 기다리는 최대 시간이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Timing", meta = (ClampMin = "1.0"))
	float InitialReadyTimeout = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Timing", meta = (ClampMin = "0.0"))
	float RoundStartDelay = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Timing", meta = (ClampMin = "0.0"))
	float RoundEndDelay = 3.0f;

	// 일시적인 Pawn 점유나 SpawnActor 실패만 이 횟수만큼 다시 시도한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Spawn", meta = (ClampMin = "0", ClampMax = "10"))
	int32 MaxSpawnPreparationRetries = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Spawn", meta = (ClampMin = "0.05"))
	float SpawnPreparationRetryDelay = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Timing", meta = (ClampMin = "1.0"))
	float RoundDuration = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Augment")
	bool bEnableAugmentDraft = true;

	// AugmentSelecting은 상점 구매와 증강 선택이 함께 열리는 30초 준비 구간이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Augment", meta = (EditCondition = "bEnableAugmentDraft", ClampMin = "0.1"))
	float AugmentSelectionDuration = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Augment", meta = (EditCondition = "bEnableAugmentDraft", ClampMin = "1", ClampMax = "8"))
	int32 AugmentOfferCount = 3;

	// 이 라운드부터 공수 역할을 교대한다. 0이면 교대하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Rules", meta = (ClampMin = "0"))
	int32 SideSwitchRound = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Rules", meta = (ClampMin = "0"))
	int32 RoundsToWin = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Rules", meta = (ClampMin = "0"))
	int32 MaxRounds = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Rules")
	ETeamRole StartingTeamARole = ETeamRole::Attackers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Rules")
	ETeamRole StartingTeamBRole = ETeamRole::Defenders;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAugmentDraftManager> AugmentDraftManager;

	UPROPERTY(Transient)
	TObjectPtr<UCreditEconomyManager> CreditEconomyManager;

	FTimerHandle PhaseTimerHandle;
	FTimerHandle RoundTimerHandle;
	FTimerHandle InitialReadyTimerHandle;
	TOptional<FRoundContext> PendingRoundContext;
	FInitialPlayerReadinessTracker InitialPlayerReadinessTracker;
	int32 LastCompletedRoundNumber = 0;
	int32 SpawnPreparationRetryCount = 0;
	bool bRoundOpeningSuspended = false;
	bool bLoggedWaitingForAugmentRegistry = false;
};
