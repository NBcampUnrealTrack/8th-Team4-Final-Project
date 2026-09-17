#pragma once

#include "CoreMinimal.h"
#include "TrainingBotPersonalityTypes.generated.h"

UENUM(BlueprintType)
enum class ETrainingBotPersonality : uint8
{
    Random,
    Copycat UMETA(DisplayName = "좌우 왕복"),
    Coward,
    Trickster
};

UENUM(BlueprintType)
enum class ETrainingBotAction : uint8
{
    WaitForParticipant,
    // 기존 StateTree와의 호환을 위해 저장된 enum 이름과 순서는 유지한다.
    CopyMirror UMETA(DisplayName = "좌우 왕복 1"),
    CopyInverse UMETA(DisplayName = "좌우 왕복 2"),
    CopyDelayed UMETA(DisplayName = "좌우 왕복 3"),
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
    FPS_API bool IsStrafeAction(ETrainingBotAction Action);
    FPS_API float TangentialProjection(const FVector& WorldVector,
        const FVector& ArenaCenter, const FVector& BotPosition);
    FPS_API bool IsAimNearCapsule(const FVector& Eye, const FVector& ViewDirection,
        const FVector& Center, float HalfHeight, float Radius, float Padding,
        FVector& OutClosestAimPoint);
}
