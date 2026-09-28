#pragma once
#include "CoreMinimal.h"
#include "Customization/CustomizationProfile.h"
#include "ReplicatedAppearance.generated.h"

USTRUCT()
struct FPS_API FReplicatedAppearance
{
    GENERATED_BODY()
    UPROPERTY() FName ClassId;
    UPROPERTY() FCustomizationAppearance Appearance;
    UPROPERTY() uint32 Revision = 0;
};
