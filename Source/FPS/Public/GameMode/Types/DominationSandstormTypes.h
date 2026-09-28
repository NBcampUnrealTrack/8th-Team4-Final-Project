#pragma once

#include "CoreMinimal.h"
#include "DominationSandstormTypes.generated.h"

UENUM(BlueprintType)
enum class EDominationSandstormPhase : uint8
{
	Inactive,
	Warning,
	FadeIn,
	Peak,
	FadeOut
};

USTRUCT(BlueprintType)
struct FPS_API FDominationSandstormState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandstorm")
	EDominationSandstormPhase Phase = EDominationSandstormPhase::Inactive;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandstorm")
	double PhaseEndServerTime = 0.0;

	// 실제 단계 길이도 보내야 늦게 접속한 클라이언트가 편집된 전환 시간을 복원한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandstorm")
	float PhaseDuration = 0.0f;

	float GetTimeRemaining(const double ServerTime) const
	{
		return static_cast<float>(FMath::Max(0.0, PhaseEndServerTime - ServerTime));
	}

	float GetIntensity(const double ServerTime) const
	{
		if (Phase == EDominationSandstormPhase::Peak)
		{
			return 1.0f;
		}
		if (Phase != EDominationSandstormPhase::FadeIn
			&& Phase != EDominationSandstormPhase::FadeOut)
		{
			return 0.0f;
		}

		const float RemainingFraction = PhaseDuration > UE_SMALL_NUMBER
			? FMath::Clamp(static_cast<float>((PhaseEndServerTime - ServerTime) / PhaseDuration), 0.0f, 1.0f)
			: 0.0f;
		const float Alpha = Phase == EDominationSandstormPhase::FadeIn
			? 1.0f - RemainingFraction
			: RemainingFraction;
		// 시각 효과와 바람 소리가 같은 Ease In/Out 곡선을 사용한다.
		return Alpha * Alpha * (3.0f - 2.0f * Alpha);
	}
};
