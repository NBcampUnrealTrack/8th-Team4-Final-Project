#pragma once
#include "CoreMinimal.h"
struct FReferenceSkeleton;

namespace AppearanceGrip
{
FPS_API bool BuildHandBasis(const FReferenceSkeleton& Source, const FReferenceSkeleton& Target,
    FName Hand, FTransform& OutBasis);
FPS_API bool BuildGripBoneBasis(const FReferenceSkeleton& Source, const FReferenceSkeleton& Target,
    FName Bone, FTransform& OutBasis);
FPS_API FTransform MapRelativeHand(const FTransform& SourceRight, const FTransform& SourceLeft,
    const FTransform& TargetRight, const FTransform& RightBasis, const FTransform& LeftBasis);
}
