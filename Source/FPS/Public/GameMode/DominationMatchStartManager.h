#pragma once

#include "CoreMinimal.h"
#include "GameMode/DominationTypes.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "DominationMatchStartManager.generated.h"

class ADominationGameMode;
class ADominationPlayerController;
class ADominationPlayerState;

DECLARE_MULTICAST_DELEGATE(FOnDominationInitialPlayersSpawned);

/**
 * 최초 참가자 확정부터 클래스 선택, 초기 스폰까지의 매치 시작 절차를 담당한다.
 * 이후 리스폰과 점령 흐름은 이 관리자에 포함하지 않는다.
 */
UCLASS()
class FPS_API UDominationMatchStartManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(
		ADominationGameMode* InOwnerGameMode,
		int32 InMinimumPlayersToStart,
		int32 InMinimumPlayersPerTeam,
		float InClassSelectionDuration);
	void Shutdown();

	void NotifyPlayerLoaded();
	void RequestCombatClass(
		ADominationPlayerController* PlayerController,
		EDominationCombatClass RequestedClass);
	void GetInitialParticipantPlayerStates(TArray<ADominationPlayerState*>& OutPlayerStates) const;

	// GameMode가 초기 거점 활성화와 외부 연결 이벤트를 조율하기 위한 내부 완료 신호다.
	FOnDominationInitialPlayersSpawned OnInitialPlayersSpawned;

private:
	bool CaptureInitialParticipants();
	bool IsInitialParticipant(const ADominationPlayerController* PlayerController) const;
	bool AreAllParticipantsClassSelected() const;
	void BeginClassSelection();
	void HandleClassSelectionTimeout();
	void CompleteClassSelection();
	bool SpawnInitialParticipants();
	class ADominationGameState* GetDominationGameState() const;

	TWeakObjectPtr<ADominationGameMode> OwnerGameMode;
	TArray<TWeakObjectPtr<ADominationPlayerController>> InitialParticipants;
	FTimerHandle ClassSelectionTimerHandle;
	int32 MinimumPlayersToStart = 2;
	int32 MinimumPlayersPerTeam = 1;
	float ClassSelectionDuration = 15.0f;
};
