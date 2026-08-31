#pragma once

#include "CoreMinimal.h"
#include "GameMode/Manager/DominationManagerBase.h"
#include "GameMode/Types/DominationTypes.h"
#include "TimerManager.h"
#include "DominationObjectiveManager.generated.h"

class ADominationCapturePoint;
class ADominationGameMode;
class ADominationGameState;
class APawn;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnDominationCaptureCompleted,
	EDominationPointId,
	EGameTeam);

/**
 * Domination 맵의 활성 거점, 점령 인원, 경합, 진행도를 관리한다.
 * 거점 Actor는 공간 감지만 하고 복제 가능한 표시 상태는 GameState에 게시한다.
 */
UCLASS()
class FPS_API UDominationObjectiveManager : public UDominationManagerBase
{
	GENERATED_BODY()

public:
	bool Initialize(
		ADominationGameMode* InOwnerGameMode,
		const FDominationCaptureRules& InCaptureRules,
		float InPointTransitionDuration);
	virtual void Shutdown() override;
	// 매치 종료 시 점령·전환 타이머와 활성 거점만 정지한다.
	void StopObjectiveFlow();
	// 중앙 C를 Repositioning으로 공개하고 전환 시간이 끝나면 점령을 활성화한다.
	bool ActivateInitialPoint();
	ADominationCapturePoint* GetCapturePoint(EDominationPointId PointId) const;

	// GameMode가 라운드 점수와 매치 종료를 처리하는 원시 점령 완료 이벤트다.
	FOnDominationCaptureCompleted OnCaptureCompleted;

private:
	bool CacheCapturePoints();
	void StartCaptureTracking(ADominationCapturePoint* CapturePoint);
	void StopCaptureTracking();
	void HandleCapturePointPawnChanged(APawn* Pawn);
	void HandleCaptureUpdateTimer();
	void HandleSuddenDeathTimerExpired();
	void BeginPointTransition(EDominationPointId CapturedPointId, EGameTeam CaptureTeam);
	void HandlePointTransitionComplete();
	void UpdateCaptureState(float DeltaSeconds);
	void CountActivePlayers(int32& OutTeamAPlayerCount, int32& OutTeamBPlayerCount) const;
	void PublishCaptureState(
		int32 TeamAPlayerCount,
		int32 TeamBPlayerCount,
		bool bContested) const;

	TArray<TWeakObjectPtr<ADominationCapturePoint>> CapturePoints;
	TWeakObjectPtr<ADominationCapturePoint> ActiveCapturePoint;
	FDominationCaptureRules CaptureRules;
	FTimerHandle CaptureUpdateTimerHandle;
	FTimerHandle SuddenDeathTimerHandle;
	FTimerHandle PointTransitionTimerHandle;
	EDominationPointId TransitionTargetPointId = EDominationPointId::None;
	EGameTeam CapturingTeam = EGameTeam::None;
	float CaptureProgress = 0.0f;
	float PointTransitionDuration = 30.0f;
	bool bCaptureCompleted = false;
	bool bSuddenDeathActive = false;
	double LastCaptureUpdateTime = 0.0;

	static constexpr float CaptureUpdateInterval = 0.1f;
};
