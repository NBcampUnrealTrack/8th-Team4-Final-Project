#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Common/BaseTeamGameMode.h"
#include "GameFlow/Round/RoundTypes.h"
#include "BaseRoundGameMode.generated.h"

class ABaseRoundGameState;
class ABaseTeamPlayerState;
class APlayerController;
class UAugmentRegistrySubsystem;

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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game Flow|Loading")
	void NotifyPlayerLoadedIntoMatch(APlayerController* PlayerController);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitGameState() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual bool ReadyToEndMatch_Implementation() override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;

	virtual void HandleGameFlowReset();
	virtual bool PrepareRound();
	virtual void HandlePreparedRoundAborted();
	virtual void HandleRoundStarted();
	virtual void HandleRoundEnded(EGameTeam WinningTeam, ERoundEndReason EndReason);
	virtual void HandlePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState);
	virtual void HandleRoundTimeExpired();
	virtual bool HasEnoughPlayersToStart() const;
	virtual bool IsGameFlowConfigurationValid() const;

	void EndRound(EGameTeam WinningTeam, ERoundEndReason EndReason);
	void SetRoundTimer(float Duration);
	void FreezeRoundTimer();
	bool PreparePlayersForRound();
	bool AreAllPlayersLoadedIntoMatch() const;
	void SetAllLoadingScreensVisible(float Progress);
	void SetAllLoadingScreenProgress(float Progress);
	void HideAllLoadingScreens();

	ABaseRoundGameState* GetBaseRoundGameState() const;
	EGameTeam GetTeamForRole(ETeamRole TeamRole) const;

private:
	void EvaluateRoundStart();
	void ScheduleRoundStart(float Delay);
	void BeginNextRound();
	void BeginAugmentSelectionPhase();
	void FinishAugmentSelectionPhase();
	void StartRoundCombat();
	void HandleRoundEndDelayFinished();
	void FinishMatch(EGameTeam WinningTeam);
	void AbortPreparedRound(bool bRollbackRoundNumber);
	void ResetAllPlayerPawns();
	void ResetAllPlayerAugments();
	void SetRoundPlayerInputEnabled(bool bEnabled);
	bool RollAugmentOffersForPlayers(int32 RoundNumber);
	bool AutoSelectAugmentOffersForPlayers();
	bool IsAugmentRegistryReady() const;
	int32 MakeRoundSeed(int32 RoundNumber) const;
	void RegenerateMatchSeed();
	bool IsMatchComplete(EGameTeam& OutWinningTeam) const;

	UFUNCTION()
	void HandleAugmentRegistryReady();

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
	FTimerHandle RoundStartTimerHandle;
	FTimerHandle AugmentSelectionTimerHandle;
	FTimerHandle RoundEndTimerHandle;
	FTimerHandle RoundTimeTimerHandle;
	TWeakObjectPtr<UAugmentRegistrySubsystem> BoundAugmentRegistry;
	TSet<TWeakObjectPtr<ABaseTeamPlayerState>> EliminatedPlayersThisRound;
	int32 MatchSeed = 0;
	bool bSidesSwitched = false;
	bool bLoggedWaitingForAugmentRegistry = false;
};
