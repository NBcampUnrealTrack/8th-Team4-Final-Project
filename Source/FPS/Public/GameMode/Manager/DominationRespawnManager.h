#pragma once

#include "CoreMinimal.h"
#include "GameMode/Manager/DominationManagerBase.h"
#include "GameMode/Types/DominationTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "DominationRespawnManager.generated.h"

class AController;
class ADominationGameMode;
class ADominationGameState;
class ADominationSpawnVolume;
class AShooterPlayerState;
class UAbilitySystemComponent;
class UDominationObjectiveManager;
struct FGameplayEventData;

/** 사망자별 리스폰 대기, 전선 기준 스폰 위치, 리스폰 직후 보호 수명주기를 관리한다. 실제 Pawn 생성은 GameMode에 요청한다. */
UCLASS()
class FPS_API UDominationRespawnManager : public UDominationManagerBase
{
	GENERATED_BODY()

public:
	bool Initialize(
		ADominationGameMode* InOwnerGameMode,
		UDominationObjectiveManager* InObjectiveManager,
		float InRespawnDelay,
		float InRespawnProtectionDuration);
	virtual void Shutdown() override;
	// 매치 종료 후 실행될 필요가 없는 예약만 취소하고 스폰 배치 캐시는 유지한다.
	void CancelPendingRespawns();
	// 매치 종료 시 남아 있는 보호 태그, 만료 타이머, 발사 이벤트 구독을 모두 해제한다.
	void ClearAllRespawnProtections();
	void ScheduleRespawn(AController* EliminatedController);
	void BeginRespawnProtection(AShooterPlayerState* PlayerState);
	bool TryGetSpawnTransform(
		EGameTeam Team,
		EDominationPointId ActivePointId,
		FTransform& OutSpawnTransform);

private:
	bool CacheSpawnVolumes();
	bool UpdateTeamSpawnVolumes(EDominationPointId ActivePointId);
	ADominationSpawnVolume* GetTeamSpawnVolume(EGameTeam Team) const;

	UFUNCTION()
	void HandleActivePointChanged(EDominationPointId ActivePointId);

	void HandleRespawnTimer(TWeakObjectPtr<AController> EliminatedController);
	void HandleRespawnProtectionExpired(TWeakObjectPtr<AShooterPlayerState> PlayerState);
	void HandleProtectedPlayerWeaponFired(
		FGameplayTag EventTag,
		const FGameplayEventData* Payload,
		TWeakObjectPtr<AShooterPlayerState> PlayerState);
	void EndRespawnProtection(AShooterPlayerState* PlayerState);

	struct FActiveRespawnProtection
	{
		TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
		FTimerHandle ExpirationTimer;
		FDelegateHandle WeaponFiredDelegate;
		FGameplayTagContainer WeaponFiredEventTags;
	};

	void ReleaseRespawnProtection(FActiveRespawnProtection& Protection);

	// 현재 상태 조회는 공통 기반을 사용하고, 델리게이트 해제를 위해 바인딩 대상만 보관한다.
	TWeakObjectPtr<ADominationGameState> BoundGameState;
	TWeakObjectPtr<UDominationObjectiveManager> ObjectiveManager;
	TArray<TWeakObjectPtr<ADominationSpawnVolume>> SpawnVolumes;
	TMap<TWeakObjectPtr<ADominationSpawnVolume>, int32> NextSpawnCandidateIndices;
	TWeakObjectPtr<ADominationSpawnVolume> TeamASpawnVolume;
	TWeakObjectPtr<ADominationSpawnVolume> TeamBSpawnVolume;
	TMap<TWeakObjectPtr<AController>, FTimerHandle> PendingRespawnTimers;
	TMap<TWeakObjectPtr<AShooterPlayerState>, FActiveRespawnProtection> ActiveRespawnProtections;
	EDominationPointId SpawnObjectivePointId = EDominationPointId::None;
	float RespawnDelay = 5.0f;
	float RespawnProtectionDuration = 2.0f;
};
