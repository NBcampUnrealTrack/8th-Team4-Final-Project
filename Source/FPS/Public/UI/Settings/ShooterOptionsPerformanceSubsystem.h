#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShooterOptionsPerformanceSubsystem.generated.h"

/**
 * Lightweight per-game-instance performance sampler used by the game options screen.
 * It records recent frame times while gameplay is active and exposes a frozen
 * snapshot to the options screen. No rendering settings are changed here.
 */
UCLASS()
class FPS_API UShooterOptionsPerformanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Pause/resume session sampling while an options menu is open. */
	void SetSamplingPaused(bool bPaused) { bSamplingPaused = bPaused; }

	/** A recommendation changes the settings, so its old measurements must not be reused. */
	void ResetSamples() { FrameTimesMs.Reset(); }

	UFUNCTION(BlueprintPure, Category = "FPS|Options|Performance")
	float GetAverageFrameTimeMs() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Options|Performance")
	float GetAverageFPS() const;

	/** Approximate 1% low FPS derived from the 99th-percentile frame time. */
	UFUNCTION(BlueprintPure, Category = "FPS|Options|Performance")
	float GetOnePercentLowFPS() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Options|Performance")
	float GetTargetMissPercent(float TargetFPS) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Options|Performance")
	int32 GetSampleCount() const { return FrameTimesMs.Num(); }

private:
	bool TickSampler(float DeltaSeconds);

	static constexpr int32 MaxSamples = 1800;
	TArray<float> FrameTimesMs;
	FTSTicker::FDelegateHandle TickerHandle;
	bool bSamplingPaused = false;
};
