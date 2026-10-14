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

	UPROPERTY(EditAnywhere, Category = "Crouch")
	TObjectPtr<UAnimSequence> Enter;

	UPROPERTY(EditAnywhere, Category = "Crouch")
	TObjectPtr<UAnimSequence> Exit;
};

// A presentation-only layer: the replicated crouch state drives one-shot lower
// body clips. Live aim/grip, locomotion, collisions and sound remain their owners.
USTRUCT(BlueprintInternalUseOnly)
struct FPS_API FAnimNode_CrouchTransition : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_CrouchTransition();

	UPROPERTY(EditAnywhere, Category = "Crouch")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Crouch")
	TMap<EDominationCombatClass, FCrouchTransitionClips> ClassClips;

	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* Instance) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void InitializeBoneReferences(const FBoneContainer& Bones) override;
	virtual bool IsValidToEvaluate(const USkeleton*, const FBoneContainer& Bones) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& Out) override;

	// Crouch keeps the standing upper-body posture. Crouch idles are authored with
	// a rotated root and twisted lower spine (about +24 degrees root yaw), which the
	// upper layers inherit as an instant torso turn/lean. The node captures the
	// standing-to-crouched spine/neck/arm offset at crouch onset (cached per class
	// and weapon) and removes it while crouched; aim, ADS and montage motion stay live.
	UPROPERTY(EditAnywhere, Category = "Crouch")
	bool bPreserveStandingUpperBody = true;

private:
	void UpdateUpperCorrection(const FQuat& InputChest, const TArray<FQuat, TInlineAllocator<8>>& InputLocal, bool bTransitionActive);

	// Lower-body transition.
	FBoneReference Pelvis;
	FBoneReference Spine;
	TArray<FCompactPoseBoneIndex> LowerBones;
	TArray<FName> LowerNames;
	TArray<bool> CachedBoneValid;
	TArray<FTransform> LastPose;
	TArray<FTransform> StartPose;
	TArray<FTransform> StartSamplePose;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> EnterClip;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ExitClip;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ActiveClip;

	bool bEligible = false;
	bool bCrouched = false;
	bool bPreviousCrouched = false;
	bool bInitializedState = false;
	bool bCaptureStart = false;
	bool bMatchReversalPose = false;

	// Standing upper-body preservation (spine_02..head and both arms, after spine_01).
	TArray<FBoneReference> UpperBones;
	TArray<FQuat> StandUpperLocal;
	TArray<FQuat> UpperDelta;
	FQuat StandChest = FQuat::Identity;
	FQuat ChestDelta = FQuat::Identity;
	FName UpperKey;
	FName StandKey;
	FName CorrectionKey;
	TWeakObjectPtr<const UObject> UpperKeyWeapon;
	int32 UpperKeyClass = -1;
	bool bUpperClean = false;
	bool bHaveStand = false;
	bool bStandClean = false;
	bool bEvalPrevCrouched = false;
	bool bUpperCorrection = false;

	float Speed = 0.f;
	float Elapsed = 0.f;
	float StartTime = 0.f;
	float Duration = 0.f;
	float MovementWeight = 1.f;
};
