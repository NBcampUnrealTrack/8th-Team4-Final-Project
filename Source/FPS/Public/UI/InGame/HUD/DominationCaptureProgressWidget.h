#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMode/Types/DominationTypes.h"
#include "TimerManager.h"
#include "DominationCaptureProgressWidget.generated.h"

class ADominationGameState;
class ADominationCapturePoint;

/**
 * DominationGameState의 복제된 점령 상태를 Blueprint 위젯에 전달한다.
 * 테스트 HUD와 최종 Main HUD가 같은 WBP를 재사용할 수 있도록 표시 방식은 Blueprint에 맡긴다.
 */
UCLASS(Abstract, Blueprintable)
class FPS_API UDominationCaptureProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Domination|Objective UI")
	EDominationPointId GetActivePointId() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective UI")
	FDominationCaptureState GetCaptureState() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective UI")
	float GetCaptureProgress() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective UI")
	EGameTeam GetCapturingTeam() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective UI")
	bool IsCaptureContested() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective UI")
	bool ShouldShowCaptureProgress() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Objective UI", meta = (DisplayName = "점령 상태 갱신"))
	void BP_OnObjectiveStateChanged(
		EDominationPointId ActivePointId,
		FDominationCaptureState CaptureState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Objective UI", meta = (DisplayName = "로컬 점령 UI 표시 상태 변경"))
	void BP_OnLocalCaptureZoneChanged(bool bShouldShow);

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Objective UI", meta = (DisplayName = "점령 게이지 표시 상태 변경"))
	void BP_OnCaptureProgressVisibilityChanged(bool bShouldShow);

	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Round UI", meta = (DisplayName = "라운드 상태 갱신"))
	void BP_OnRoundStateChanged(
		int32 CurrentRoundNumber,
		int32 TeamAScore,
		int32 TeamBScore);

private:
	UFUNCTION()
	void HandleActivePointChanged(EDominationPointId ActivePointId);

	UFUNCTION()
	void HandleCaptureStateChanged(FDominationCaptureState CaptureState);

	UFUNCTION()
	void HandleObjectiveFlowStateChanged();

	UFUNCTION()
	void HandleTeamScoreChanged(EGameTeam Team, int32 NewScore);

	UFUNCTION()
	void HandleDominationFlowStateChanged();

	void RefreshBlueprint();
	void RefreshRoundBlueprint();
	void RefreshActiveCapturePoint();
	void RefreshLocalCaptureZoneState();
	void RefreshCaptureProgressVisibility();

	TWeakObjectPtr<ADominationGameState> BoundGameState;
	TWeakObjectPtr<ADominationCapturePoint> ActiveCapturePoint;
	FTimerHandle LocalCaptureZoneTimerHandle;
	bool bHasLocalCaptureZoneState = false;
	bool bLocalPlayerInsideActivePoint = false;
	bool bHasCaptureProgressVisibilityState = false;
	bool bCaptureProgressVisible = false;

	static constexpr float LocalCaptureZoneUpdateInterval = 0.1f;
};
