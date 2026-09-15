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
    CombatResponseActive
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

namespace TrainingRules
{
    inline constexpr float MovementRadiusCentimeters = 300.0f;
    inline constexpr float TargetIntervalSeconds = 2.0f;
    inline constexpr float TargetDurationSeconds = 60.0f;
    inline constexpr float CombatIntervalSeconds = 5.0f;
    inline constexpr float CombatHealth = 100.0f;
    inline constexpr float CorpseLifetimeSeconds = 3.0f;
    inline constexpr int32 MaxLivingCombatAI = 10;

    FPS_API bool ResolveDistanceBand(ETrainingDistancePreset Preset,
        float CustomMeters, FTrainingDistanceBand& OutBand);
    FPS_API bool CanSpawnCombatAI(int32 LivingCount);
}
