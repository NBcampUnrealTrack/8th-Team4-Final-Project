#pragma once
#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "GameMode/Types/DominationTypes.h"
#include "AnimNode_AlignLowerBody.generated.h"

USTRUCT()
struct FPS_API FPrimaryLowerBodyStance
{
 GENERATED_BODY()
 // Pelvis is component-space; leg entries are parent-local transforms.
 UPROPERTY(EditAnywhere,Category="Alignment") TMap<FName,FTransform> Bones;
};

USTRUCT(BlueprintInternalUseOnly)
struct FPS_API FAnimNode_AlignLowerBody : public FAnimNode_SkeletalControlBase
{
 GENERATED_BODY()
 FAnimNode_AlignLowerBody();
 UPROPERTY(EditAnywhere,Category="Alignment") bool bEnabled=true;
 UPROPERTY(EditAnywhere,Category="Alignment") TMap<EDominationCombatClass,FPrimaryLowerBodyStance> PrimaryStances;
 virtual bool HasPreUpdate() const override {return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
 virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
 virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
 virtual void InitializeBoneReferences(const FBoneContainer& Bones) override;
 virtual bool IsValidToEvaluate(const USkeleton*,const FBoneContainer& Bones) override;
 virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out) override;
private:
 FBoneReference Pelvis,Spine;
 EDominationCombatClass CombatClass=EDominationCombatClass::Unselected;
 bool bEligible=false;
};
