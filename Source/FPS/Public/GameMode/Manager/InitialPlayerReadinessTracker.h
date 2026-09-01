#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UWorld;

/** 현재 팀에 배정된 플레이어들의 최초 로딩 준비 상태. */
struct FPS_API FInitialPlayerReadinessSnapshot
{
	int32 ReadyPlayerCount = 0;
	int32 AssignedPlayerCount = 0;
	TArray<TWeakObjectPtr<APlayerController>> AssignedPlayerControllers;
	TArray<FString> UnreadyPlayerNames;

	bool AreAllAssignedPlayersReady(int32 MinimumPlayerCount) const;
};

/**
 * 첫 라운드 UI 준비 신호만 보관하는 서버 측 값 객체.
 * 타이머와 페이즈 전이는 ABaseRoundGameMode가 계속 담당한다.
 */
class FPS_API FInitialPlayerReadinessTracker
{
public:
	// 동일 맵 재경기는 로딩을 마친 플레이어의 준비 상태를 유지한다.
	void ResetTimeoutForSameMapRestart();

	bool MarkPlayerReady(APlayerController* PlayerController);
	void RemovePlayer(APlayerController* PlayerController);
	bool IsPlayerReady(APlayerController* PlayerController) const;

	void MarkTimeoutExpired();
	bool HasTimeoutExpired() const;

	FInitialPlayerReadinessSnapshot CaptureSnapshot(const UWorld* World) const;

private:
	TSet<TWeakObjectPtr<APlayerController>> ReadyPlayerControllers;
	bool bTimeoutExpired = false;
};
