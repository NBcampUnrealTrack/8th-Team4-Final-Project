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
    Idle,
    TargetAcquisitionActive,
    CombatResponseActive,
    Countdown
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
    inline constexpr float MinDistanceMeters = 5.0f;
    inline constexpr float MaxDistanceMeters = 50.0f;
    inline constexpr float DistanceHalfWidthMeters = 2.0f;
    inline constexpr float CentimetersPerMeter = 100.0f;
    inline constexpr float BotArcStepCentimeters = 150.0f;
    inline constexpr float MovementRadiusCentimeters = 300.0f;
    inline constexpr float TargetIntervalSeconds = 2.0f;
    inline constexpr float TargetMinIntervalSeconds = 1.0f;
    inline constexpr float TargetDurationSeconds = 60.0f;
    inline constexpr float CombatIntervalSeconds = 5.0f;
    inline constexpr float TargetHealth = 1.0f;
    inline constexpr float CombatHealth = 100.0f;
    inline constexpr float CountdownSeconds = 3.0f;
    inline constexpr float DissolveDurationSeconds = 3.0f;
    inline constexpr int32 MaxLivingCombatAI = 10;

    FPS_API bool ResolveDistanceBand(ETrainingDistancePreset Preset,
        float CustomMeters, FTrainingDistanceBand& OutBand);
    FPS_API int32 RemainingWholeSeconds(float Duration, float Elapsed);
    FPS_API float GetTargetSpawnIntervalSeconds(float Elapsed, float Duration);
    FPS_API bool CanSpawnCombatAI(int32 LivingCount);
    FPS_API bool IsRunningState(ETrainingSessionState State);
    FPS_API float FeedbackOpacity(float Elapsed, float Duration, float FadeIn, float FadeOut);
}
