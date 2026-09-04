#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationObjectiveFeedbackWidget.generated.h"

class ADominationGameState;

/**
 * DominationGameState의 복제된 ObjectiveFlowState를 관찰해 거점 전환 피드백을 Blueprint에 전달한다.
 * 게임 규칙/네트워크 상태는 GameState가 소유하고, 문구/색상/애니메이션은 WBP가 담당한다.
 */
UCLASS(Abstract, Blueprintable)
class FPS_API UDominationObjectiveFeedbackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Domination|Objective Feedback")
	FDominationObjectiveFlowState GetObjectiveFlowState() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 최초 중앙 거점이 열리기 전 안내다. 점령 팀/이전 거점이 없으므로 별도 이벤트로 분리한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Objective Feedback", meta = (DisplayName = "초기 거점 전환 시작"))
	void BP_OnInitialObjectiveTransitionStarted(
		EDominationPointId NextPointId,
		int32 TransitionDurationSeconds);

	// 실제 점령 완료 시 한 번 발생한다. 다음 거점이 없으면 bHasNextObjective=false다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Objective Feedback", meta = (DisplayName = "거점 점령 피드백 시작"))
	void BP_OnObjectiveCaptured(
		EGameTeam CapturingTeam,
		EDominationPointId CapturedPointId,
		EDominationPointId NextPointId,
		bool bHasNextObjective,
		int32 TransitionDurationSeconds);

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Objective Feedback", meta = (DisplayName = "거점 전환 종료"))
	void BP_OnObjectiveTransitionEnded(EDominationPointId ActivePointId);

private:
	UFUNCTION()
	void HandleObjectiveFlowStateChanged();

	void RefreshObjectiveFeedback(bool bInitialBinding);

	TWeakObjectPtr<ADominationGameState> BoundGameState;
	int32 LastHandledCaptureSerial = INDEX_NONE;
	bool bTransitionPresented = false;
};
