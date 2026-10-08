#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMode/Types/DominationTypes.h"
#include "GameMode/Types/TeamTypes.h"
#include "TimerManager.h"
#include "DominationSiteTrackerWidget.generated.h"

class ADominationGameState;
class ADominationCapturePoint;
class APawn;

/**
 * A-B-C-D-E 5개 거점을 한눈에 보여주는 인디케이터 (O-O-O-O-O, 활성 거점만 강조).
 * 양옆 라운드 점수, 활성 거점의 점령 진행률/점령 팀도 함께 제공
 * DominationCaptureProgressWidget과 동일하게 표시 방식(강조 모양, 테두리 채움, 중앙 정렬 애니메이션)은
 * Blueprint에 맡기고, C++은 ADominationGameState의 복제 상태만 전달
 */

UCLASS(Abstract, Blueprintable)
class FPS_API UDominationSiteTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	EDominationPointId GetActivePointId() const;

	// 0부터 시작하는 인덱스. 활성 거점이 없으면 INDEX_NONE(-1)
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	int32 GetActivePointIndex() const;

	// 항상 DominationRules::PointCount(5)
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	int32 GetPointCount() const;

	// A, B, C, D, E 순서 그대로. Blueprint에서 반복문으로 아이콘 5개를 만들 때 사용
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	TArray<EDominationPointId> GetAllPointIds() const;

	// 활성 거점의 점령 진행률. 0.0~1.0, 네모 테두리를 이 비율만큼 채우면 됨
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	float GetCaptureProgress() const;

	// 현재 점령 중인 팀 (절대 팀 A/B). 색상 판단에는 GetCapturingRelation()을 쓸 것
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	EGameTeam GetCapturingTeam() const;

	// 로컬 플레이어 기준 아군/적/중립. 인디케이터 색 판단용
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	EDominationCaptureRelation GetCapturingRelation() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	bool IsCaptureContested() const;

	// 인디케이터 양옆에 띄울 라운드 점수
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	int32 GetTeamAScore() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	int32 GetTeamBScore() const;

	// 로컬 플레이어가 지금 활성 거점 구역 안에 살아서 들어와 있는지
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	bool IsLocalPlayerInsideActivePoint() const;

	// true면 점령 완료 후 다음 거점 활성화 대기(전환) 중 - GetTransitionSecondsRemaining()을 표시
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	bool IsPointTransitioning() const;

	// 다음 거점이 활성화되기까지 남은 시간(초, 올림). 전환 중이 아니면 0
	UFUNCTION(BlueprintPure, Category = "Domination|Site Tracker")
	int32 GetTransitionSecondsRemaining() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 활성 거점이 바뀔 때마다 호출. Blueprint는 여기서 네모 강조를 옮기고 중앙 정렬 애니메이션을 재생한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Site Tracker", meta = (DisplayName = "활성 거점 변경"))
	void BP_OnActivePointChanged(EDominationPointId NewActivePointId, int32 NewActivePointIndex);

	// 점령 진행률/점령 팀이 바뀔 때마다 호출. Blueprint는 여기서 테두리 채움 비율과 색을 갱신한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Site Tracker", meta = (DisplayName = "점령 진행 변경"))
	void BP_OnCaptureStateChanged(EDominationCaptureRelation CapturingRelation, float CaptureProgress, bool bContested);

	// 라운드 점수가 바뀔 때마다 호출
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Site Tracker", meta = (DisplayName = "라운드 점수 변경"))
	void BP_OnRoundScoreChanged(int32 TeamAScore, int32 TeamBScore);

	// 내 팀 기준 점수. 점수나 내 팀이 바뀔 때 호출. 왼쪽에 Ally, 오른쪽에 Enemy 를 그리면 된다
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Site Tracker", meta = (DisplayName = "내 팀 기준 점수 변경"))
	void BP_OnRelativeScoreChanged(int32 AllyScore, int32 EnemyScore);

	// 로컬 플레이어가 활성 거점 구역에 들어오거나 나갈 때 호출. Blueprint는 여기서 강조 크기를 키우거나 줄이면 된다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Site Tracker", meta = (DisplayName = "로컬 점령 구역 진입 상태 변경"))
	void BP_OnLocalCaptureZoneChanged(bool bShouldShow);

	// 다음 거점 활성화 대기(전환) 상태가 시작/종료될 때 1회 호출.
	// 이 시점 이후 매초 남은 시간을 갱신하려면 Blueprint에서 자체 타이머로 GetTransitionSecondsRemaining()을 폴링할 것
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Site Tracker", meta = (DisplayName = "거점 전환 대기 상태 변경"))
	void BP_OnTransitionStateChanged(bool bIsTransitioning);

private:
	UFUNCTION()
	void HandleActivePointChanged(EDominationPointId ActivePointId);

	UFUNCTION()
	void HandleCaptureStateChanged(FDominationCaptureState CaptureState);

	UFUNCTION()
	void HandleTeamScoreChanged(EGameTeam Team, int32 NewScore);

	UFUNCTION()
	void HandleObjectiveFlowStateChanged();

	void HandleCapturePointPawnChanged(APawn* Pawn);

	void RefreshActiveCapturePoint();
	void RefreshLocalCaptureZoneState();
	void RefreshLocalState();
	// bForce 가 false 면 내 팀이 바뀌었을 때만 보낸다
	void RefreshRelativeScore(bool bForce);

	TWeakObjectPtr<ADominationGameState> BoundGameState;
	TWeakObjectPtr<ADominationCapturePoint> ActiveCapturePoint;
	FTimerHandle LocalCaptureZoneTimerHandle;
	bool bHasLocalCaptureZoneState = false;
	bool bLocalPlayerInsideActivePoint = false;
	EGameTeam CachedLocalTeam = EGameTeam::None;

	static constexpr float LocalCaptureZoneUpdateInterval = 0.1f;
};
