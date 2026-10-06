#pragma once

#include "CoreMinimal.h"

class UGroomComponent;
class UGroomAsset;
class UGroomBindingAsset;
class USceneComponent;

// Reusable live parts only; authored fallback retirement remains a separate policy.
namespace AppearanceGroomLifecycle
{
    FPS_API void Clear(UGroomComponent& Component);
    FPS_API void Activate(UGroomComponent& Component, USceneComponent& Parent,
        UGroomAsset& Groom, UGroomBindingAsset& Binding, bool bUseCards);
}
