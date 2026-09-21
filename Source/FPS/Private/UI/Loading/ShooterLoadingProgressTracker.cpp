// ShooterLoadingProgressTracker.cpp
#include "UI/Loading/ShooterLoadingProgressTracker.h"

void FShooterLoadingProgressTracker::SetTarget(const float InRawProgress)
{
	TargetProgress = FMath::Max(TargetProgress, FMath::Clamp(InRawProgress, 0.0f, 1.0f));
}

bool FShooterLoadingProgressTracker::Tick(
	const float DeltaSeconds,
	const FShooterLoadingProgressSettings& Settings)
{
	const float Previous = DisplayProgress;

	const float Delta = TargetProgress - DisplayProgress;
	if (Delta <= Settings.SettleEpsilon)
	{
		DisplayProgress = TargetProgress;
		return DisplayProgress != Previous;
	}

	// Hitch 방어를 위함. 긴 Frame 이후 진행률이 한 번에 튀는 것을 막기 위해 제한
	const float EffectiveDT = FMath::Min(DeltaSeconds, Settings.MaxInterpolationDeltaTime);

	const float InterpSpeed = FMath::IsNearlyEqual(TargetProgress, 1.0f)
		? Settings.CompletionInterpSpeed
		: Settings.ProgressInterpSpeed;

	// 프레임레이트 독립
	const float Alpha = 1.0f - FMath::Exp(-InterpSpeed * EffectiveDT);

	const float Step = FMath::Min(
		Delta * Alpha,
		Settings.MaxVisualProgressPerSecond * EffectiveDT);

	DisplayProgress += Step;

	return DisplayProgress != Previous;
}

void FShooterLoadingProgressTracker::Reset()
{
	TargetProgress = 0.0f;
	DisplayProgress = 0.0f;
}

bool FShooterLoadingProgressTracker::IsVisuallyComplete(const FShooterLoadingProgressSettings& Settings) const
{
	return DisplayProgress >= Settings.CompletionVisualThreshold;
}
