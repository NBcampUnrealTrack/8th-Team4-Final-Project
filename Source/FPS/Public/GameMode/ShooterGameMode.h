// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Player/PlayerParticipationState.h"
#include "ShooterGameMode.generated.h"

class AController;
class APlayerController;
class ASpectatorPawn;
class AShooterSpectatorPawn;
class AShooterPlayerState;
struct FRogueDeathContext;

/**
 * 
 */
UCLASS()
class FPS_API AShooterGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AShooterGameMode();

	// Character의 서버 사망 통지를 받는 단일 진입점.
	// 킬로그를 먼저 발신한 뒤 모드별 규칙으로 넘긴다.
	void HandlePlayerEliminated(
		AController* EliminatedController,
		const FRogueDeathContext& DeathContext);

	// 로컬 플레이어의 맵/UI 준비 완료를 모드별 시작 게이트로 전달한다.
	virtual void NotifyPlayerReadyForGameFlow(APlayerController* PlayerController);

	void CycleSpectatorTarget(APlayerController* SpectatorController);

	// 트레일러 촬영용 카메라 클라이언트를 기존 관전 흐름으로 옮기는 진입점(개발 전용).
	// MovePlayerToSpectator는 protected라서 컨트롤러가 직접 부를 수 없어 공개 래퍼를 둔다.
	void EnterSpectatorForTrailerCamera(APlayerController* PlayerController);

protected:
	// 사망 통지를 실제 게임 규칙으로 전달한다.
	virtual void NotifyPlayerEliminatedBy(
		AController* EliminatedController,
		AShooterPlayerState* KillerPlayerState);

	// 사망 컨텍스트에서 킬 크레딧을 가져갈 플레이어를 고른다.
	static AShooterPlayerState* ResolveKillerPlayerState(const FRogueDeathContext& DeathContext);

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual EPlayerParticipationState ResolveParticipationStateForNewPlayer(APlayerController* NewPlayer) const;
	void InitializePlayerParticipationState(APlayerController* NewPlayer) const;
	void MovePlayerToSpectator(APlayerController* PlayerController);
	ASpectatorPawn* SpawnSpectatorPawnFor(APlayerController* PlayerController, const FTransform& SpawnTransform);
	FTransform ResolveSpectatorSpawnTransform(APlayerController* PlayerController) const;
	APawn* FindNextSpectatorTarget(APlayerController* SpectatorController) const;
	void ScheduleSpectatorTargetRetry(APlayerController* SpectatorController);
	void ClearSpectatorTargetRetry(TWeakObjectPtr<APlayerController> WeakController);
	void RetrySpectatorTarget(TWeakObjectPtr<APlayerController> SpectatorController);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Spectator", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AShooterSpectatorPawn> ShooterSpectatorPawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Spectator", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float SpectatorTargetRetryInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Spectator", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", Units = "s"))
	float CorpseLifeSpanSeconds = 5.0f;

	TMap<TWeakObjectPtr<APlayerController>, FTimerHandle> SpectatorTargetRetryTimers;
};
