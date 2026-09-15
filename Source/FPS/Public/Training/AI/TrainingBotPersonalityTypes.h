#pragma once

#include "CoreMinimal.h"
#include "TrainingBotPersonalityTypes.generated.h"

UENUM(BlueprintType)
enum class ETrainingBotPersonality : uint8
{
    Random,
    Copycat,
    Coward,
    Trickster
};

UENUM(BlueprintType)
enum class ETrainingBotAction : uint8
{
    WaitForParticipant,
    CopyMirror,
    CopyInverse,
    CopyDelayed,
    CowardApproach,
    CowardEscape,
    TrickCruise,
    TrickFeint,
    TrickCommit,
    TrickPause
};

UENUM()
enum class ETrainingBotCondition : uint8
{
    PersonalityIs,
    HasParticipant,
    IsBeingAimedAt
};

struct FPS_API FTrainingVelocityHistory
{
    static constexpr int32 Capacity = 64;

    void Reset();
    void Add(float TimeSeconds, const FVector& Velocity);
    bool TryGetAtOrBefore(float TimeSeconds, FVector& OutVelocity) const;

private:
    struct FSample
    {
        float TimeSeconds = 0.0f;
        FVector Velocity = FVector::ZeroVector;
    };

    FSample Samples[Capacity];
    int32 NextIndex = 0;
    int32 SampleCount = 0;
};

struct FPS_API FTrainingAimLatch
{
    void Reset();
    void Update(bool bObserved, float DeltaSeconds, float ReactionSeconds);
    bool IsAimedAt() const { return bAimedAt; }

private:
    bool bAimedAt = false;
    float PendingSeconds = 0.0f;
};

namespace TrainingPersonality
{
    FPS_API bool IsConcretePersonality(ETrainingBotPersonality Personality);
    FPS_API bool IsCopyAction(ETrainingBotAction Action);
    FPS_API float TangentialVelocity(const FVector& WorldVelocity,
        const FVector& ArenaCenter, const FVector& BotPosition);
    FPS_API bool IsAimNearCapsule(const FVector& Eye, const FVector& ViewDirection,
        const FVector& Center, float HalfHeight, float Radius, float Padding,
        FVector& OutClosestAimPoint);
}
