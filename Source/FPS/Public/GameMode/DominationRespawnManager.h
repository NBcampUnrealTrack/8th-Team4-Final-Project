#pragma once

#include "CoreMinimal.h"
#include "GameMode/DominationTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "UObject/Object.h"
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
class FPS_API UDominationRespawnManager : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(
		ADominationGameMode* InOwnerGameMode,
		UDominationObjectiveManager* InObjectiveManager,
		float InRespawnDelay,
		float InRespawnProtectionDuration);
	void Shutdown();
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

	TWeakObjectPtr<ADominationGameMode> OwnerGameMode;
	TWeakObjectPtr<ADominationGameState> DominationGameState;
	TWeakObjectPtr<UDominationObjectiveManager> ObjectiveManager;
	TArray<TWeakObjectPtr<ADominationSpawnVolume>> SpawnVolumes;
	TWeakObjectPtr<ADominationSpawnVolume> TeamASpawnVolume;
	TWeakObjectPtr<ADominationSpawnVolume> TeamBSpawnVolume;
	TMap<TWeakObjectPtr<AController>, FTimerHandle> PendingRespawnTimers;
	TMap<TWeakObjectPtr<AShooterPlayerState>, FActiveRespawnProtection> ActiveRespawnProtections;
	EDominationPointId SpawnObjectivePointId = EDominationPointId::None;
	float RespawnDelay = 5.0f;
	float RespawnProtectionDuration = 2.0f;
};
