#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Common/BaseTeamGameMode.h"
#include "GameFlow/Round/RoundTypes.h"
#include "BaseRoundGameMode.generated.h"

class ABaseRoundGameState;
class ABaseTeamPlayerState;
class APlayerController;
class UAugmentDraftManager;

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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game Flow|Match")
	void RestartGameFlow();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game Flow|Round")
	void RetryRoundOpening();

	// 각 로컬 플레이어의 인게임 UI가 준비되면 첫 라운드 시작 조건을 다시 평가한다.
	void NotifyPlayerReadyForGameFlow(APlayerController* PlayerController);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitGameState() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual bool ReadyToEndMatch_Implementation() override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;

	// 파생 모드는 목표물 수명주기와 승패 규칙만 구현한다.
	virtual void ResetModeState();
	virtual bool PrepareModeRound(const FRoundContext& RoundContext);
	virtual bool ActivateModeRound();
	virtual void DeactivateModeRound(EGameTeam WinningTeam, ERoundEndReason EndReason);
	virtual void HandleModePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState);
	virtual void ResolveModeRoundTimeExpired();
	virtual bool HasModeRequiredPlayers() const;
	virtual bool IsModeConfigurationValid() const;

	void EndRound(EGameTeam WinningTeam, ERoundEndReason EndReason);
	void FreezeRoundTimer();

	ABaseRoundGameState* GetBaseRoundGameState() const;
	ETeamRole GetRoleForTeam(EGameTeam Team) const;
	EGameTeam GetTeamForRole(ETeamRole TeamRole) const;
	ETeamRole GetSpawnRoleForTeam(EGameTeam Team) const;

private:
	void InitializeGameFlow();
	void HandlePlayerRosterChanged();
	void HandleDraftDependenciesReady();

	// 상위 흐름은 아래 전이만 따라가면 된다.
	void TryBeginNextRound();       // Waiting/Ready -> RoundStarting
	void OpenNextRound();           // RoundStarting -> Draft 또는 Combat
	void FinishAugmentDraft();      // Draft -> Combat
	bool TryStartRoundCombat();     // 준비 완료 -> RoundInProgress
	void AdvanceToNextRound();      // RoundEnding -> Waiting
	void FinishMatch(EGameTeam WinningTeam);

	void EnterWaitingForPlayers();
	void EnterWaitingForPlayersReady();
	void SuspendRoundOpening(const TCHAR* Reason);
	FRoundContext MakeRoundContext(int32 RoundNumber) const;

	bool HasRequiredPlayersToStart() const;
	bool AreInitialPlayersReadyForGameFlow() const;
	bool IsGameFlowConfigurationValid() const;
	bool IsBaseConfigurationValid() const;
	bool IsMatchComplete(EGameTeam& OutWinningTeam) const;

	void StartRoundTimer(float Duration);
	void HandleRoundTimerExpired();
	void ClearPhaseTimer();
	void ClearRoundTimer();
	void ClearAllFlowTimers();

	void SetAllPlayersRoundInputEnabled(bool bEnabled);
	void SetRoundParticipantInputEnabled(bool bEnabled);
	void SetPlayerRoundInputEnabled(APlayerController* PlayerController, bool bEnabled) const;
	UAugmentDraftManager* GetAugmentDraftManager() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Players", meta = (ClampMin = "1"))
	int32 MinimumPlayersToStart = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Timing", meta = (ClampMin = "0.0"))
	float InitialRoundStartDelay = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Timing", meta = (ClampMin = "0.0"))
	float RoundStartDelay = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Timing", meta = (ClampMin = "0.0"))
	float RoundEndDelay = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Timing", meta = (ClampMin = "1.0"))
	float RoundDuration = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Augment")
	bool bEnableAugmentDraft = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Augment", meta = (EditCondition = "bEnableAugmentDraft", ClampMin = "0.1"))
	float AugmentSelectionDuration = 10.0f;

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

	FTimerHandle PhaseTimerHandle;
	FTimerHandle RoundTimerHandle;
	TOptional<FRoundContext> PendingRoundContext;
	TSet<TWeakObjectPtr<APlayerController>> InitialReadyPlayerControllers;
	int32 LastCompletedRoundNumber = 0;
	bool bRoundOpeningSuspended = false;
	bool bLoggedWaitingForAugmentRegistry = false;
};
