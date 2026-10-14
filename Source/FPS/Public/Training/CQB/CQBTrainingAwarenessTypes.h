#pragma once

#include "CoreMinimal.h"
#include "CQBTrainingAwarenessTypes.generated.h"

UENUM(BlueprintType)
enum class ECQBThreatKnowledge : uint8
{
    Unknown,
    Suspected,
    LastKnown,
    Confirmed,
};

USTRUCT(BlueprintType)
struct FPS_API FCQBAwarenessSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Awareness", meta = (ClampMin = "0.0", Units = "cm"))
    float AllyDeathAwarenessRadius = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Awareness", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
    float DirectionalUncertaintyDegrees = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Awareness", meta = (Units = "cm"))
    FVector2D ShooterEstimateDistance = FVector2D(400.0f, 1100.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Awareness", meta = (ClampMin = "1.0", Units = "s"))
    float EvidenceMemorySeconds = 12.0f;
};
