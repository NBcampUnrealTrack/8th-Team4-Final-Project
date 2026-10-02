#include "Customization/AppearancePoseAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Customization/AppearanceGrip.h"
#include "TwoBoneIK.h"

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
        // Ragdoll pelvis translation includes compensation for the moving physics
        // root. Reference-pose adaptation rotates that large translation on bodies
        // with different rest axes (Mason -> Gavin), making the visible corpse
        // drift away from its settled physics bodies. Copy final component-space
        // physics transforms instead; keep anatomical adaptation for living poses.
        bRetarget = Pose->bPreserveReferencePose &&
            !(Pose->PoseSource && Pose->PoseSource->IsAnySimulatingPhysics());
        Copy.bUseMeshPose = !bRetarget;
        USkeletalMesh* Source = Pose->PoseSource ? Pose->PoseSource->GetSkeletalMeshAsset() : nullptr;
        USkeletalMesh* Target = Instance->GetSkelMeshComponent()->GetSkeletalMeshAsset();
        if (Source != CachedSource.Get() || Target != CachedTarget.Get())
        {
            CachedSource = Source; CachedTarget = Target; SourceIndices.Reset(); SourceRefs.Reset(); TargetRefs.Reset();
            bValidHandBasis = false;
            if (Source && Target)
            {
                SourceRefs = Source->GetRefSkeleton().GetRefBonePose();
                TargetRefs = Target->GetRefSkeleton().GetRefBonePose();
                for (int32 I = 0; I < TargetRefs.Num(); ++I)
                    SourceIndices.Add(Source->GetRefSkeleton().FindBoneIndex(Target->GetRefSkeleton().GetBoneName(I)));
                bValidHandBasis = AppearanceGrip::BuildHandBasis(Source->GetRefSkeleton(), Target->GetRefSkeleton(), TEXT("hand_r"), RightBasis) &&
                    AppearanceGrip::BuildHandBasis(Source->GetRefSkeleton(), Target->GetRefSkeleton(), TEXT("hand_l"), LeftBasis);
            }
        }
        // CopyPose maps every named bone, including bones skipped by the source
        // LOD. Those component transforms may be left over from an earlier pose.
        // Snapshot the evaluated set on the game thread; never read it in Evaluate.
        SourceEvaluated.Init(false, SourceRefs.Num());
        if (Pose->PoseSource)
            for (const FBoneIndexType Bone : Pose->PoseSource->RequiredBones)
                if (SourceEvaluated.IsValidIndex(Bone)) SourceEvaluated[Bone] = true;
        bSolveHands = bRetarget && Pose->bPreserveWeaponHandRelationship && bValidHandBasis &&
            Pose->PoseSource && !Pose->PoseSource->IsAnySimulatingPhysics() &&
            !Instance->GetSkelMeshComponent()->IsAnySimulatingPhysics();
        if (bSolveHands)
        {
            SourceRight = Pose->PoseSource->GetSocketTransform(TEXT("hand_r"), RTS_Component);
            SourceLeft = Pose->PoseSource->GetSocketTransform(TEXT("hand_l"), RTS_Component);
        }
        Copy.PreUpdate(Instance);
    }
    virtual void CacheBones() override { Copy.CacheBones_AnyThread(FAnimationCacheBonesContext(this)); }
    virtual void Update(float Delta) override { Copy.Update_AnyThread(FAnimationUpdateContext(this, Delta)); }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Copy.Evaluate_AnyThread(Output);
        for (FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
        {
            const int32 MeshBone = Output.Pose.GetBoneContainer().MakeMeshPoseIndex(Bone).GetInt();
            if (!SourceIndices.IsValidIndex(MeshBone)) continue;
            const int32 SourceBone = SourceIndices[MeshBone];
            if (SourceEvaluated.IsValidIndex(SourceBone) && !SourceEvaluated[SourceBone])
            {
                // Rebuild omitted children locally from their moving parent rather
                // than copying stale world/component positions (stretched gloves).
                Output.Pose[Bone] = bRetarget ? SourceRefs[SourceBone] : TargetRefs[MeshBone];
            }
        }
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
        if (bSolveHands)
        {
            const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
            auto Find = [&](FName Name) { return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetReferenceSkeleton().FindBoneIndex(Name))); };
            const FCompactPoseBoneIndex Upper = Find(TEXT("upperarm_l")), Lower = Find(TEXT("lowerarm_l")),
                Hand = Find(TEXT("hand_l")), Right = Find(TEXT("hand_r"));
            if (Upper.IsValid() && Lower.IsValid() && Hand.IsValid() && Right.IsValid())
            {
                FCSPose<FCompactPose> ComponentPose;
                ComponentPose.InitPose(Output.Pose);
                // Local reference-pose adaptation can rotate the weapon palm
                // away from the authored aim (Mason -> Gavin is about 20 deg).
                // Preserve the authored palm orientation without moving the
                // target wrist or stretching its arm. Finger motion stays local.
                FTransform RightCS = ComponentPose.GetComponentSpaceTransform(Right);
                RightCS.SetRotation((SourceRight.GetRotation() * RightBasis.GetRotation().Inverse()).GetNormalized());
                const FCompactPoseBoneIndex RightParent = Output.Pose.GetParentBoneIndex(Right);
                Output.Pose[Right] = RightParent.IsValid()
                    ? RightCS.GetRelativeTransform(ComponentPose.GetComponentSpaceTransform(RightParent)) : RightCS;
                Output.Pose[Right].NormalizeRotation();
                FTransform UpperCS = ComponentPose.GetComponentSpaceTransform(Upper);
                FTransform LowerCS = ComponentPose.GetComponentSpaceTransform(Lower);
                FTransform HandCS = ComponentPose.GetComponentSpaceTransform(Hand);
                const FTransform Desired = AppearanceGrip::MapRelativeHand(SourceRight, SourceLeft,
                    RightCS, RightBasis, LeftBasis);
                // No stretch: retain the body's proportions, and preserve intentional
                // Draw/reload hand separation by following the source relative motion.
                AnimationCore::SolveTwoBoneIK(UpperCS, LowerCS, HandCS, LowerCS.GetLocation(),
                    Desired.GetLocation(), false, 1.0, 1.0);
                HandCS.SetRotation(Desired.GetRotation());
                const FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Upper);
                Output.Pose[Upper] = Parent.IsValid() ? UpperCS.GetRelativeTransform(ComponentPose.GetComponentSpaceTransform(Parent)) : UpperCS;
                Output.Pose[Lower] = LowerCS.GetRelativeTransform(UpperCS);
                Output.Pose[Hand] = HandCS.GetRelativeTransform(LowerCS);
                Output.Pose[Upper].NormalizeRotation(); Output.Pose[Lower].NormalizeRotation(); Output.Pose[Hand].NormalizeRotation();
            }
        }
        return true;
    }
private:
    FAnimNode_CopyPoseFromMesh Copy;
    TWeakObjectPtr<USkeletalMesh> CachedSource, CachedTarget;
    TArray<int32> SourceIndices;
    TBitArray<> SourceEvaluated;
    TArray<FTransform> SourceRefs, TargetRefs;
    bool bRetarget = true;
    bool bValidHandBasis = false, bSolveHands = false;
    FTransform RightBasis, LeftBasis, SourceRight, SourceLeft;
};

FAnimInstanceProxy* UAppearancePoseAnimInstance::CreateAnimInstanceProxy() { return new FAppearancePoseProxy(this); }
void UAppearancePoseAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
