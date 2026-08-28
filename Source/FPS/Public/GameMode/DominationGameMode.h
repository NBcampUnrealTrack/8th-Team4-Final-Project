#pragma once

#include "CoreMinimal.h"
#include "GameMode/BaseTeamGameMode.h"
#include "GameMode/DominationTypes.h"
#include "TimerManager.h"
#include "DominationGameMode.generated.h"

class APlayerController;
class AShooterCharacter;
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

// Domination의 시작, 거점, 리스폰을 연결하고 라운드 점수와 최종 승리를 판정한다.
UCLASS()
class FPS_API ADominationGameMode : public ABaseTeamGameMode
{
	GENERATED_BODY()

public:
	ADominationGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyPlayerEliminatedBy(
		AController* EliminatedController,
		AShooterPlayerState* KillerPlayerState) override;
	virtual void NotifyPlayerReadyForGameFlow(APlayerController* PlayerController) override;

	void RequestCombatClass(
		ADominationPlayerController* PlayerController,
		EDominationCombatClass RequestedClass);
	UDominationLoadoutManager* GetLoadoutManager() const;
	void GetInitialParticipantPlayerStates(
		TArray<ADominationPlayerState*>& OutPlayerStates) const;
	bool RestartPlayerAtObjective(
		APlayerController* PlayerController,
		EDominationPointId ObjectivePointId);
	void RespawnEliminatedPlayer(AController* EliminatedController);

	// 초기 참가자 스폰과 중앙 거점 활성화가 끝난 뒤 발생한다.
	// 외부 증강 시스템은 Manager 내부가 아니라 이 GameMode 경계에 연결한다.
	FOnDominationMatchStarted OnDominationMatchStarted;

	// 인자: 점령 거점, 라운드 승리 팀, 해당 팀 누적 점수, 매치 승리 여부.
	// 외부 보상·증강 시스템은 점수 반영이 끝난 이 이벤트에 연결한다.
	FOnDominationRoundCompleted OnRoundCompleted;

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
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
	void HandleCaptureCompleted(
		EDominationPointId CapturedPointId,
		EGameTeam CapturingTeam);
	void FinishDominationMatch(EGameTeam WinningTeam);
	void ReturnToLobbyAfterMatch();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Class Pawn", meta = (AllowPrivateAccess = "true"))
	TMap<EDominationCombatClass, TSubclassOf<AShooterCharacter>> CombatClassPawnClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Match Start", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "4"))
	int32 RequiredPlayersPerTeam = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Match Start", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float ClassSelectionDuration = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true"))
	FDominationCaptureRules CaptureRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float PointTransitionDuration = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Respawn", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", Units = "s"))
	float RespawnDelay = 5.0f;

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

	FTimerHandle MatchEndTimerHandle;
};
