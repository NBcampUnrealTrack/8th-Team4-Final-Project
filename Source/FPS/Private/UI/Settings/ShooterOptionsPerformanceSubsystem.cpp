#include "UI/Settings/ShooterOptionsPerformanceSubsystem.h"

#include "Algo/Sort.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UShooterOptionsPerformanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FrameTimesMs.Reserve(MaxSamples);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickSampler));
}

void UShooterOptionsPerformanceSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	FrameTimesMs.Reset();
	Super::Deinitialize();
}

bool UShooterOptionsPerformanceSubsystem::TickSampler(const float DeltaSeconds)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (bSamplingPaused || !World || !World->HasBegunPlay() || World->IsPaused() ||
		!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f || DeltaSeconds > 0.5f)
	{
		return true;
	}

	const float FrameMs = DeltaSeconds * 1000.0f;
	if (FrameTimesMs.Num() >= MaxSamples)
	{
		FrameTimesMs.RemoveAt(0, FrameTimesMs.Num() - MaxSamples + 1, EAllowShrinking::No);
	}
	FrameTimesMs.Add(FrameMs);
	return true;
}

float UShooterOptionsPerformanceSubsystem::GetAverageFrameTimeMs() const
{
	if (FrameTimesMs.IsEmpty())
	{
		return 0.0f;
	}

	double Sum = 0.0;
	for (const float Value : FrameTimesMs)
	{
		Sum += Value;
	}
	return static_cast<float>(Sum / static_cast<double>(FrameTimesMs.Num()));
}

float UShooterOptionsPerformanceSubsystem::GetAverageFPS() const
{
	const float AverageMs = GetAverageFrameTimeMs();
	return AverageMs > KINDA_SMALL_NUMBER ? 1000.0f / AverageMs : 0.0f;
}

float UShooterOptionsPerformanceSubsystem::GetOnePercentLowFPS() const
{
	if (FrameTimesMs.Num() < 10)
	{
		return GetAverageFPS();
	}

	TArray<float> Sorted = FrameTimesMs;
	Sorted.Sort();
	const int32 Index = FMath::Clamp(
		FMath::FloorToInt(static_cast<float>(Sorted.Num() - 1) * 0.99f),
		0,
		Sorted.Num() - 1);
	const float P99FrameMs = Sorted[Index];
	return P99FrameMs > KINDA_SMALL_NUMBER ? 1000.0f / P99FrameMs : 0.0f;
}

float UShooterOptionsPerformanceSubsystem::GetTargetMissPercent(const float TargetFPS) const
{
	if (FrameTimesMs.IsEmpty() || TargetFPS <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float TargetFrameMs = 1000.0f / TargetFPS;
	int32 MissCount = 0;
	for (const float FrameMs : FrameTimesMs)
	{
		if (FrameMs > TargetFrameMs)
		{
			++MissCount;
		}
	}

	return 100.0f * static_cast<float>(MissCount) / static_cast<float>(FrameTimesMs.Num());
}
