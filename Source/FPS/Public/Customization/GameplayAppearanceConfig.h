#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAppearanceConfig.generated.h"

class UCustomizationStudioConfig;
class USkeletalMesh;
class UAnimInstance;

USTRUCT(BlueprintType)
struct FPS_API FGameplayAppearanceBody
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString BodyType;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh> FaceMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftClassPtr<UAnimInstance> FaceAnimClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform FaceTransform;
};

// Rendering catalog only. Server validation uses CustomizationClassRules and
// never loads this catalog's meshes, Mutable object, materials or grooms.
UCLASS(BlueprintType)
class FPS_API UGameplayAppearanceConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ClassId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UCustomizationStudioConfig> StudioConfig;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FGameplayAppearanceBody> Bodies;
};
