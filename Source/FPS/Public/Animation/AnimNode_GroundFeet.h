#pragma once
#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_GroundFeet.generated.h"

// A shared, additive terrain adjustment. It samples the input pose, never feeds
// yesterday's corrected Z back into the solver, and never stretches limb bones.
USTRUCT(BlueprintInternalUseOnly)
struct FPS_API FAnimNode_GroundFeet : public FAnimNode_SkeletalControlBase
{
 GENERATED_BODY()
 FAnimNode_GroundFeet();
 UPROPERTY(EditAnywhere,Category="Grounding") FBoneReference Pelvis;
 UPROPERTY(EditAnywhere,Category="Grounding") FBoneReference LeftFoot;
 UPROPERTY(EditAnywhere,Category="Grounding") FBoneReference RightFoot;
 UPROPERTY(EditAnywhere,Category="Grounding",meta=(ClampMin="0",ClampMax="25")) float MaxFootOffset=15.f;
 UPROPERTY(EditAnywhere,Category="Grounding",meta=(ClampMin="0",ClampMax="15")) float MaxPelvisOffset=10.f;
 // Reference-pose sole plane in mesh space, not a character scale adjustment.
 UPROPERTY(EditAnywhere,Category="Grounding") float ReferenceSoleZ=0.f;
 virtual bool HasPreUpdate() const override {return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
 virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
 virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
 virtual bool IsValidToEvaluate(const USkeleton*,const FBoneContainer& Bones) override;
 virtual void InitializeBoneReferences(const FBoneContainer& Bones) override;
 virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out) override;
private:
 friend struct FGroundFeetTestAccess;
 friend struct FGroundFeetWorldTestAccess;
 FBoneReference Knees[2],Thighs[2];
 float ReferenceFootZ[2]={0,0};
 FVector BendDirection[2]={FVector::RightVector,FVector::RightVector};
 FVector FloorPoint[2],FloorNormal[2];
 bool bHit[2]={false,false},bGrounded=false;
 FTransform MeshWorld;
 float GroundWeight=0,Delta=0,PelvisOffset=0,Offsets[2]={0,0};
};
