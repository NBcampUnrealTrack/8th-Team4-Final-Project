#include "Customization/AppearancePoseAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

class FAppearancePoseProxy : public FAnimInstanceProxy
{
public:
    explicit FAppearancePoseProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
    {
        Copy.bCopyCurves = true;
        Copy.bUseAttachedParent = false;
        Copy.bUseMeshPose = true;
    }
    virtual void Initialize(UAnimInstance* Instance) override
    {
        FAnimInstanceProxy::Initialize(Instance);
        Copy.Initialize_AnyThread(FAnimationInitializeContext(this));
    }
    virtual void PreUpdate(UAnimInstance* Instance, float Delta) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, Delta);
        const auto* Pose = CastChecked<UAppearancePoseAnimInstance>(Instance);
        Copy.SourceMeshComponent = Pose->PoseSource;
        bRetarget = Pose->bPreserveReferencePose;
        Copy.bUseMeshPose = !bRetarget;
        USkeletalMesh* Source = Pose->PoseSource ? Pose->PoseSource->GetSkeletalMeshAsset() : nullptr;
        USkeletalMesh* Target = Instance->GetSkelMeshComponent()->GetSkeletalMeshAsset();
        if (Source != CachedSource.Get() || Target != CachedTarget.Get())
        {
            CachedSource = Source; CachedTarget = Target; SourceIndices.Reset(); SourceRefs.Reset(); TargetRefs.Reset();
            if (Source && Target)
            {
                SourceRefs = Source->GetRefSkeleton().GetRefBonePose();
                TargetRefs = Target->GetRefSkeleton().GetRefBonePose();
                for (int32 I = 0; I < TargetRefs.Num(); ++I)
                    SourceIndices.Add(Source->GetRefSkeleton().FindBoneIndex(Target->GetRefSkeleton().GetBoneName(I)));
            }
        }
        Copy.PreUpdate(Instance);
    }
    virtual void CacheBones() override { Copy.CacheBones_AnyThread(FAnimationCacheBonesContext(this)); }
    virtual void Update(float Delta) override { Copy.Update_AnyThread(FAnimationUpdateContext(this, Delta)); }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Copy.Evaluate_AnyThread(Output);
        if (bRetarget)
            for (FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
            {
                const int32 MeshBone = Output.Pose.GetBoneContainer().MakeMeshPoseIndex(Bone).GetInt();
                if (!SourceIndices.IsValidIndex(MeshBone)) continue;
                const int32 SourceBone = SourceIndices[MeshBone];
                if (SourceRefs.IsValidIndex(SourceBone))
                {
                    Output.Pose[Bone] = Output.Pose[Bone].GetRelativeTransform(SourceRefs[SourceBone]) * TargetRefs[MeshBone];
                    Output.Pose[Bone].NormalizeRotation();
                }
            }
        return true;
    }
private:
    FAnimNode_CopyPoseFromMesh Copy;
    TWeakObjectPtr<USkeletalMesh> CachedSource, CachedTarget;
    TArray<int32> SourceIndices;
    TArray<FTransform> SourceRefs, TargetRefs;
    bool bRetarget = true;
};

FAnimInstanceProxy* UAppearancePoseAnimInstance::CreateAnimInstanceProxy() { return new FAppearancePoseProxy(this); }
void UAppearancePoseAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
