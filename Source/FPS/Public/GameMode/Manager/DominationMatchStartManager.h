#pragma once

#include "CoreMinimal.h"
#include "GameMode/Manager/DominationManagerBase.h"
#include "GameMode/Types/DominationTypes.h"
#include "TimerManager.h"
#include "DominationMatchStartManager.generated.h"

class APlayerController;
class ADominationGameMode;
class ADominationPlayerController;
class ADominationPlayerState;

DECLARE_MULTICAST_DELEGATE(FOnDominationInitialPlayersSpawned);

/**
 * 최초 참가자 확정부터 클래스 선택, 초기 스폰까지의 매치 시작 절차를 담당한다.
 * 이후 리스폰과 점령 흐름은 이 관리자에 포함하지 않는다.
 */
UCLASS()
class FPS_API UDominationMatchStartManager : public UDominationManagerBase
{
	GENERATED_BODY()

public:
	bool Initialize(
		ADominationGameMode* InOwnerGameMode,
		int32 InMinimumPlayersToStart,
		int32 InMinimumPlayersPerTeam,
		float InClassSelectionDuration);
	virtual void Shutdown() override;

	void NotifyPlayerLoaded();
	void NotifyPlayerLeaving(APlayerController* PlayerController);
	void RetryMatchStart();
	void RequestCombatClass(
		ADominationPlayerController* PlayerController,
		EDominationCombatClass RequestedClass);
	// 임시 선택은 매치 시작 조건이 아니므로 전원 확정 검사를 하지 않는다.
	void RequestPreviewCombatClass(
		ADominationPlayerController* PlayerController,
		EDominationCombatClass RequestedClass);
	void GetInitialParticipantPlayerStates(TArray<ADominationPlayerState*>& OutPlayerStates) const;
	void NotifyClientInitialCombatAssetsReady(
		ADominationPlayerController* PlayerController, FGuid Generation, bool bSucceeded);

	// GameMode가 초기 거점 활성화와 외부 연결 이벤트를 조율하기 위한 내부 완료 신호다.
	FOnDominationInitialPlayersSpawned OnInitialPlayersSpawned;

private:
	enum class EInitialSpawnResult : uint8
	{
		Succeeded,
		AssetsLoading,
		TemporarilyBlocked,
		InvalidConfiguration
	};

	bool CaptureInitialParticipants(const APlayerController* ExcludedPlayer = nullptr);
	bool HasRequiredParticipants() const;
	void ResetStartAttempt();
	void SuspendMatchStart(const TCHAR* Reason);
	void CleanupInitialSpawns();
	bool IsInitialParticipant(const ADominationPlayerController* PlayerController) const;
	bool AreAllParticipantsClassSelected() const;
	void BeginClassSelection();
	void HandleClassSelectionTimeout();
	void CompleteClassSelection();
	void HandleCombatAssetsReady(EDominationCombatClass CombatClass, bool bSucceeded);
	bool EnsureClientCombatAssets();
	void ResetClientCombatAssetPreparation();
	EInitialSpawnResult SpawnInitialParticipants();

	TArray<TWeakObjectPtr<ADominationPlayerController>> InitialParticipants;
	TSet<TWeakObjectPtr<ADominationPlayerController>> PendingClientCombatAssets;
	FGuid ClientCombatAssetGeneration;
	bool bClientCombatAssetsRequested = false;
	FTimerHandle ClassSelectionTimerHandle;
	int32 MinimumPlayersToStart = 2;
	int32 MinimumPlayersPerTeam = 1;
	float ClassSelectionDuration = 15.0f;
	int32 SpawnRetryCount = 0;
	bool bStartSuspended = false;
	bool bWaitingForCombatAssets = false;
	static constexpr int32 MaxSpawnRetries = 3;
	static constexpr float SpawnRetryDelay = 0.5f;
};
