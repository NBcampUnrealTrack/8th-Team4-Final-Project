#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

class UCustomizationStudioConfig;
class USkeletalMesh;
struct FStudioGroomChoice;

// Selection is metadata-only. Loading and ownership belong to each appearance request.
namespace AppearanceGroomSelection
{
    FPS_API bool Select(const UCustomizationStudioConfig& Config, const FString& BodyType,
        FName HairId, TArray<FStudioGroomChoice>& OutChoices, FString& OutError);
    FPS_API void GatherPaths(const TArray<FStudioGroomChoice>& Choices, TArray<FSoftObjectPath>& OutPaths);
    FPS_API bool ValidateLoaded(const TArray<FStudioGroomChoice>& Choices,
        USkeletalMesh* BindingSurface, FString& OutError);
}
