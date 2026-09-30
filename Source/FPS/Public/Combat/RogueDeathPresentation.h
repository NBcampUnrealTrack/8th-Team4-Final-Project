#pragma once

#include "CoreMinimal.h"

class ACharacter;
struct FRogueDeathReaction;

// Shared mesh-only presentation. Health/death rules stay in URogueHealthComponent.
struct FPS_API FRogueRagdollSettings
{
    float ImpulseSpeed = 600.0f;
    float HeadshotImpulseMultiplier = 1.8f;
    float UpwardBias = 0.25f;
    FName ImpulseBone = TEXT("pelvis");
    FName HeadshotImpulseBone = TEXT("head");
};

namespace RogueDeathPresentation
{
    FPS_API void RefreshRagdollFollowers(ACharacter& Character);
    FPS_API void StartRagdoll(ACharacter& Character, const FRogueDeathReaction& Reaction,
        const FRogueRagdollSettings& Settings);
}
