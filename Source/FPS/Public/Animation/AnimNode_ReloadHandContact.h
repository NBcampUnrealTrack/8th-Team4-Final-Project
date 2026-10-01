#pragma once
#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "GameplayTagContainer.h"
#include "AnimNode_ReloadHandContact.generated.h"

class UAnimMontage;
struct FReloadContactPlayback;

USTRUCT()
struct FPS_API FReloadHandPath
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,Category="Contact") FGameplayTag WeaponTag;
 // Uniform normalized-time samples, in third-person weapon component space.
 UPROPERTY(EditAnywhere,Category="Contact") TArray<FTransform> Samples;
};

USTRUCT(BlueprintInternalUseOnly)
struct FPS_API FAnimNode_ReloadHandContact : public FAnimNode_SkeletalControlBase
{
 GENERATED_BODY()
 FAnimNode_ReloadHandContact();
 UPROPERTY(EditAnywhere,Category="Contact") bool bEnabled=true;
 UPROPERTY(EditAnywhere,Category="Contact") TMap<TObjectPtr<UAnimMontage>,FReloadHandPath> Paths;
 virtual bool HasPreUpdate() const override {return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
 virtual void InitializeBoneReferences(const FBoneContainer& Bones) override;
 virtual bool IsValidToEvaluate(const USkeleton*,const FBoneContainer& Bones) override;
 virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out) override;
private:
 FBoneReference Upper,Lower,Hand,Grip;
 FTransform WeaponInParentGrip=FTransform::Identity;
 FTransform RightBasis=FTransform::Identity,LeftBasis=FTransform::Identity;
 FGameplayTag CachedWeaponTag;
 bool bAttachmentReady=false;
 const TArray<FReloadContactPlayback>* Playback=nullptr;
};
