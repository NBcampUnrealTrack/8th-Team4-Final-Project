#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMode/DominationTypes.h"
#include "TimerManager.h"
#include "DominationObjectiveTransitionTimerWidget.generated.h"

class ADominationGameState;

/**
 * ObjectiveFlowState의 Repositioning 종료 시각을 로컬에서 표시한다.
 * 서버는 종료 시각만 복제하고, 각 클라이언트는 동기화된 서버 시간으로 남은 시간을 계산한다.
 * 순간 알림인 ObjectiveFeedbackWidget과 달리 전환 상태가 유지되는 동안 지속 표시만 담당한다.
 */
UCLASS(Abstract, Blueprintable)
class FPS_API UDominationObjectiveTransitionTimerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Domination|Objective Timer")
	float GetTransitionTimeRemaining() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Objective Timer", meta = (DisplayName = "거점 전환 타이머 시작"))
	void BP_OnTransitionTimerStarted(
		EDominationPointId TargetPointId,
		int32 SecondsRemaining);

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Objective Timer", meta = (DisplayName = "거점 전환 타이머 갱신"))
	void BP_OnTransitionTimerUpdated(
		EDominationPointId TargetPointId,
		int32 SecondsRemaining);

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Objective Timer", meta = (DisplayName = "거점 전환 타이머 종료"))
	void BP_OnTransitionTimerEnded(EDominationPointId TargetPointId);

private:
	UFUNCTION()
	void HandleObjectiveFlowStateChanged();

	void RefreshTransitionState();
	void RefreshCountdown();
	void StartLocalUpdateTimer();
	void StopLocalUpdateTimer();

	TWeakObjectPtr<ADominationGameState> BoundGameState;
	FTimerHandle CountdownUpdateTimerHandle;
	EDominationPointId PresentedPointId = EDominationPointId::None;
	int32 LastPresentedSeconds = INDEX_NONE;
	bool bTimerPresented = false;

	static constexpr float CountdownUpdateInterval = 0.1f;
};
