#pragma once

#include "CoreMinimal.h"
#include "TrainingTypes.generated.h"

UENUM(BlueprintType)
enum class ETrainingMode : uint8
{
    TargetAcquisition,
    CombatResponse
};

UENUM(BlueprintType)
enum class ETrainingSessionState : uint8
{
    Idle = 0,
    TargetAcquisitionActive = 1 UMETA(Hidden),
    CombatResponseActive = 2 UMETA(Hidden),
    Countdown = 3,
    Active = 4
};

UENUM(BlueprintType)
enum class ETrainingFeedbackPhase : uint8
{
    Countdown,
    Started,
    Stopped
};

UENUM(BlueprintType)
enum class ETrainingDistancePreset : uint8
{
    TenMeters,
    TwentyMeters,
    ThirtyMeters,
    Custom
};

UENUM(BlueprintType)
enum class ETrainingInteractionRole : uint8
{
    Entrance,
    ControlPanel
};

UENUM(BlueprintType)
enum class ETrainingDialogKind : uint8
{
    EnterArena,
    SelectMode,
    EndSession
};

USTRUCT(BlueprintType)
struct FPS_API FTrainingDistanceBand
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    float MinCentimeters = 1800.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    float MaxCentimeters = 2200.0f;

    bool Contains(const FVector& Center, const FVector& Position) const;
    bool ContainsSegment(const FVector& Center, const FVector& Start, const FVector& End) const;
};

USTRUCT(BlueprintType)
struct FPS_API FTrainingSessionStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    int32 Kills = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    int32 ShotsFired = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    int32 ShotsHit = 0;

    void RecordShot(bool bHitTrainingBot);
    float GetAccuracyPercent() const;
};

USTRUCT(BlueprintType)
struct FPS_API FTrainingSessionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    int32 SessionId = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    ETrainingMode Mode = ETrainingMode::TargetAcquisition;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    FTrainingSessionStats Stats;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    float ElapsedSeconds = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Training")
    bool bCompleted = false;
};

namespace TrainingRules
{
    // Unit conversion is not gameplay tuning; editable training values live in UTrainingSettings.
    inline constexpr float CentimetersPerMeter = 100.0f;

    FPS_API bool ResolveDistanceBand(ETrainingDistancePreset Preset,
        float CustomMeters, FTrainingDistanceBand& OutBand);
    FPS_API int32 RemainingWholeSeconds(float Duration, float Elapsed);
    FPS_API bool IsRunningState(ETrainingSessionState State);
    FPS_API float FeedbackOpacity(float Elapsed, float Duration, float FadeIn, float FadeOut);
}
