// ShooterLoadingProgressTracker.h
#pragma once

#include "CoreMinimal.h"
#include "ShooterLoadingProgressTracker.generated.h"

// 로딩 진행률 보간 튜닝값
USTRUCT()
struct FShooterLoadingProgressSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Loading", meta = (ClampMin = "0.1"))
	float ProgressInterpSpeed = 2.0f;

	// Target 이 1.0 일 때만 사용
	UPROPERTY(EditDefaultsOnly, Category = "Loading", meta = (ClampMin = "0.1"))
	float CompletionInterpSpeed = 12.0f;

	// 초당 시각 진행량 상한
	UPROPERTY(EditDefaultsOnly, Category = "Loading", meta = (ClampMin = "0.01"))
	float MaxVisualProgressPerSecond = 0.35f;

	// 보간에 쓰는 DeltaTime 상한
	UPROPERTY(EditDefaultsOnly, Category = "Loading", meta = (ClampMin = "0.016", ClampMax = "0.5"))
	float MaxInterpolationDeltaTime = 0.1f;

	// 이보다 가까우면 Target 으로 정착시
	UPROPERTY(EditDefaultsOnly, Category = "Loading", meta = (ClampMin = "0.0001"))
	float SettleEpsilon = 0.001f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Loading", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float CompletionVisualThreshold = 0.999f;

	// 100% 에 닿은 뒤 더 보여주는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Loading", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CompletionHoldSeconds = 0.15f;

	// 수렴이 안 끝나도 이 시간이 지나면 걷는다
	UPROPERTY(EditDefaultsOnly, Category = "Loading", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float MaxCompletionWaitSeconds = 1.0f;
};

// 받는 값(Raw)과 그리는 값(Display)을 분리한다
struct FPS_API FShooterLoadingProgressTracker
{
	void SetTarget(float InRawProgress);

	// Display 가 바뀌었으면 true
	bool Tick(float DeltaSeconds, const FShooterLoadingProgressSettings& Settings);

	void Reset();

	float GetDisplay() const { return DisplayProgress; }
	float GetTarget() const { return TargetProgress; }

	// 화면이 100% 에 도달했는가
	bool IsVisuallyComplete(const FShooterLoadingProgressSettings& Settings) const;

private:
	float TargetProgress = 0.0f;
	float DisplayProgress = 0.0f;
};
