#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationObjectiveCompassWidget.generated.h"

class ADominationGameState;
class ADominationCapturePoint;

/**
 * 현재 목표 거점(ADominationGameState::GetActivePointId())을 향한 화면 마커.
 * - 화면 안에 있으면 실제 투영 위치를, 화면 밖(또는 카메라 뒤)이면 화면 가장자리에 클램프된
 *   위치와 화살표 회전각을 제공한다 (실제 클램핑/투영 계산은 C++에서 수행, 모양/배치는 Blueprint 몫).
 * - 목표 거점의 점령 구역 안에 로컬 플레이어가 들어가 있으면 표시하지 않는다 (ShouldShowMarker == false).
 * - Repositioning(다음 거점 활성화 대기, 30초) 중에는 IsTransitioning()==true와 함께 GetSecondsRemaining()을,
 *   그 외(점령 가능 상태)에는 GetDistanceMeters()를 보여주면 된다.
 */

UCLASS(Abstract, Blueprintable)
class FPS_API UDominationObjectiveCompassWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 목표 거점 알파벳(A~E). 목표가 없으면 None
	UFUNCTION(BlueprintPure, Category = "Domination|Compass")
	EDominationPointId GetTargetPointId() const;

	// 지금 마커를 표시해야 하는지 (목표가 없거나, 목표 점령 구역 안에 있으면 false)
	UFUNCTION(BlueprintPure, Category = "Domination|Compass")
	bool ShouldShowMarker() const;

	// true면 화면 안(실제 투영 위치), false면 화면 밖이라 가장자리에 클램프된 상태
	UFUNCTION(BlueprintPure, Category = "Domination|Compass")
	bool IsOnScreen() const;

	// 뷰포트 절대 좌표(px, 좌상단 기준)
	UFUNCTION(BlueprintPure, Category = "Domination|Compass")
	FVector2D GetScreenPosition() const;

	// 화면 밖일 때 화살표가 향해야 할 각도(도, 0=+X축 기준 시계방향). 화면 안이면 0
	UFUNCTION(BlueprintPure, Category = "Domination|Compass")
	float GetArrowRotationDegrees() const;

	// true면 다음 거점 활성화 대기(전환) 중 - GetSecondsRemaining()을 표시
	// false면 점령 가능 상태 - GetDistanceMeters()를 표시
	UFUNCTION(BlueprintPure, Category = "Domination|Compass")
	bool IsTransitioning() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Compass")
	int32 GetSecondsRemaining() const;

	// 로컬 플레이어까지 거리(미터)
	UFUNCTION(BlueprintPure, Category = "Domination|Compass")
	float GetDistanceMeters() const;

	// 인디케이터 색(아군=초록/적=빨강/중립=흰색) 판단용. 점령 가능 상태에서만 의미 있음(전환 중엔 Neutral)
	UFUNCTION(BlueprintPure, Category = "Domination|Compass")
	EDominationCaptureRelation GetCapturingRelation() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Compass")
	float GetCaptureProgress() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleActivePointChanged(EDominationPointId ActivePointId);

	void RefreshTargetCapturePoint();
	void RefreshScreenPlacement();

	TWeakObjectPtr<ADominationGameState> BoundGameState;
	TWeakObjectPtr<ADominationCapturePoint> TargetCapturePoint;

	bool bShouldShow = false;
	bool bIsOnScreen = false;
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	float ArrowRotationDegrees = 0.0f;
	float DistanceMeters = -1.0f;

	// 화면 가장자리에서 아이콘을 얼마나 안쪽으로 클램프할지 (px). 아이콘 크기의 절반 정도 권장.
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Compass")
	float EdgePaddingPixels = 64.0f;
};
