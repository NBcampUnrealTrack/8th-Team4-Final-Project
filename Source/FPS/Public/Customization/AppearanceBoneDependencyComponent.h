#pragma once
#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "AppearanceBoneDependencyComponent.generated.h"

// Non-rendering dependency provider for authored LeaderPose and generated CopyPose
// meshes. Keeps required source bones without changing target pose ownership.
UCLASS(Transient)
class FPS_API UAppearanceBoneDependencyComponent : public USkeletalMeshComponent
{
    GENERATED_BODY()
public:
    UAppearanceBoneDependencyComponent();
    void Configure(USkeletalMeshComponent* Source, const TArray<USkeletalMeshComponent*>& Targets);
    virtual void GetAdditionalRequiredBonesForLeader(int32 LODIndex, TArray<FBoneIndexType>& Bones) const override;
protected:
    virtual void OnUnregister() override;
private:
    TWeakObjectPtr<USkeletalMeshComponent> PoseSource;
    TArray<TWeakObjectPtr<USkeletalMeshComponent>> PoseTargets;
};
