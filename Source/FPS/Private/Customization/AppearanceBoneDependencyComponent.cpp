#include "Customization/AppearanceBoneDependencyComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"

UAppearanceBoneDependencyComponent::UAppearanceBoneDependencyComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetVisibility(false);
    SetRenderInMainPass(false);
    SetCastShadow(false);
}

void UAppearanceBoneDependencyComponent::Configure(USkeletalMeshComponent* Source,
    const TArray<USkeletalMeshComponent*>& Targets)
{
    PoseSource = Source;
    PoseTargets.Reset();
    for (auto* Target : Targets) if (Target && Target != Source) PoseTargets.Add(Target);
    // Only this empty provider is registered here; visible targets keep their
    // authored LeaderPose/CopyPose setup. The engine owns the required-bone hook.
    SetLeaderPoseComponent(Source, true, false);
    // A new generated mesh can need different bones even if the source/LOD did
    // not change, so invalidate the source's cached required-bone list explicitly.
    if (Source) Source->bRequiredBonesUpToDate = false;
}

void UAppearanceBoneDependencyComponent::GetAdditionalRequiredBonesForLeader(
    int32 LODIndex, TArray<FBoneIndexType>& Bones) const
{
    const auto* Source = PoseSource.Get();
    const auto* SourceMesh = Source ? Source->GetSkeletalMeshAsset() : nullptr;
    if (!SourceMesh) return;
    TArray<FBoneIndexType> Additional;
    for (const auto& TargetPtr : PoseTargets)
    {
        const auto* Target = TargetPtr.Get();
        const auto* Mesh = Target ? Target->GetSkeletalMeshAsset() : nullptr;
        const auto* Data = Mesh ? Mesh->GetResourceForRendering() : nullptr;
        if (!Data || Data->LODRenderData.IsEmpty()) continue;
        // LODSync has already selected this frame's render LOD, before the target
        // has ticked and updated its predicted LOD. A one-LOD outfit stays at zero.
        const int32 Requested = Target->GetForcedLOD() > 0
            ? Target->GetForcedLOD() - 1 : Target->GetPredictedLODLevel();
        const int32 TargetLOD = FMath::Clamp(Requested, 0, Data->LODRenderData.Num() - 1);
        for (const FBoneIndexType TargetBone : Data->LODRenderData[TargetLOD].RequiredBones)
        {
            const int32 SourceBone = SourceMesh->GetRefSkeleton().FindBoneIndex(Mesh->GetRefSkeleton().GetBoneName(TargetBone));
            if (SourceBone != INDEX_NONE) Additional.AddUnique(SourceBone);
        }
    }
    Additional.Sort();
    MergeInBoneIndexArrays(Bones, Additional);
    // ComputeRequiredBones subsequently adds parent bones and keeps sorted order.
}

void UAppearanceBoneDependencyComponent::OnUnregister()
{
    SetLeaderPoseComponent(nullptr, true, false);
    PoseSource.Reset();
    PoseTargets.Reset();
    Super::OnUnregister();
}
