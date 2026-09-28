#pragma once

#include "CoreMinimal.h"
#include "CQBTrainingTacticalTypes.generated.h"

UENUM(BlueprintType)
enum class ECQBTacticalIntent : uint8
{
    DirectFire,
    TakeCover,
    HoldAngle,
    PeekFire,
    Reposition
};

USTRUCT(BlueprintType)
struct FPS_API FCQBTacticalSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Tactics", meta = (ClampMin = "150.0", ClampMax = "1200.0", Units = "cm"))
    float CoverSearchRadius = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Tactics", meta = (ClampMin = "0.2", Units = "s"))
    float ReevaluationIntervalSeconds = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Tactics", meta = (ClampMin = "0.5", Units = "s"))
    float HoldAngleSeconds = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Tactics", meta = (ClampMin = "0.3", ClampMax = "3.0", Units = "s"))
    float PeekExposureSeconds = 1.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Tactics", meta = (ClampMin = "0.5", Units = "s"))
    float RepositionCooldownSeconds = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Tactics", meta = (ClampMin = "1", ClampMax = "8"))
    int32 MaxBurstsPerPosition = 3;
};

struct FCQBCoverPosition
{
    FVector Location = FVector::ZeroVector;
    FVector LeftPeek = FVector::ZeroVector;
    FVector RightPeek = FVector::ZeroVector;
    float Quality = 0.0f;
    float PathLength = 0.0f;
    bool bLeftPeek = false;
    bool bRightPeek = false;
    bool bValid = false;
};
