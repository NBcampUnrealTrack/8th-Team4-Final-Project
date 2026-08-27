#pragma once

#include "CoreMinimal.h"
#include "GameMode/DominationTypes.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "DominationRespawnManager.generated.h"

class AController;
class ADominationGameMode;
class ADominationSpawnVolume;
class UDominationObjectiveManager;

/** 사망자별 대기시간과 전선 기준 스폰 위치 선택을 관리한다. 실제 Pawn 생성은 GameMode에 요청한다. */
UCLASS()
class FPS_API UDominationRespawnManager : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(
		ADominationGameMode* InOwnerGameMode,
		UDominationObjectiveManager* InObjectiveManager,
		float InRespawnDelay);
	void Shutdown();
	// 매치 종료 후 실행될 필요가 없는 예약만 취소하고 스폰 배치 캐시는 유지한다.
	void CancelPendingRespawns();
	void ScheduleRespawn(AController* EliminatedController);
	bool TryGetSpawnTransform(
		EGameTeam Team,
		EDominationPointId ActivePointId,
		FTransform& OutSpawnTransform);

private:
	bool CacheSpawnVolumes();
	void HandleRespawnTimer(TWeakObjectPtr<AController> EliminatedController);

	TWeakObjectPtr<ADominationGameMode> OwnerGameMode;
	TWeakObjectPtr<UDominationObjectiveManager> ObjectiveManager;
	TArray<TWeakObjectPtr<ADominationSpawnVolume>> SpawnVolumes;
	TMap<TWeakObjectPtr<AController>, FTimerHandle> PendingRespawnTimers;
	float RespawnDelay = 5.0f;
};
