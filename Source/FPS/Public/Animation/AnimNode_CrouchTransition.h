#pragma once
#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "GameMode/Types/DominationTypes.h"
#include "AnimNode_CrouchTransition.generated.h"

class UAnimSequence;

USTRUCT()
struct FPS_API FCrouchTransitionClips
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,Category="Crouch") TObjectPtr<UAnimSequence> Enter;
 UPROPERTY(EditAnywhere,Category="Crouch") TObjectPtr<UAnimSequence> Exit;
};

// A presentation-only layer: the replicated crouch state drives one-shot lower
// body clips. Live aim/grip, locomotion, collisions and sound remain their owners.
USTRUCT(BlueprintInternalUseOnly)
struct FPS_API FAnimNode_CrouchTransition : public FAnimNode_SkeletalControlBase
{
 GENERATED_BODY()
 FAnimNode_CrouchTransition();
 UPROPERTY(EditAnywhere,Category="Crouch") bool bEnabled=true;
 UPROPERTY(EditAnywhere,Category="Crouch") TMap<EDominationCombatClass,FCrouchTransitionClips> ClassClips;
 virtual bool HasPreUpdate() const override {return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
 virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
 virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
 virtual void InitializeBoneReferences(const FBoneContainer& Bones) override;
 virtual bool IsValidToEvaluate(const USkeleton*,const FBoneContainer& Bones) override;
 virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out) override;
private:
 FBoneReference Pelvis,Spine;
 TArray<FCompactPoseBoneIndex> LowerBones;
 TArray<FName> LowerNames;
 TArray<bool> CachedBoneValid;
 TArray<FTransform> LastPose,StartPose;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> EnterClip;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> ExitClip;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> ActiveClip;
 bool bEligible=false,bCrouched=false,bPreviousCrouched=false,bInitializedState=false,bCaptureStart=false;
 float Speed=0,Elapsed=0,StartTime=0,Duration=0,MovementWeight=1;
};
