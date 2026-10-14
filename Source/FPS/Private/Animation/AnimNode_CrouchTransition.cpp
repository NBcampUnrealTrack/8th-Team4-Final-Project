#include "Animation/AnimNode_CrouchTransition.h"
#include "CrouchReversalSampling.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Weapon/WeaponBase.h"
#include "Misc/ScopeLock.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Player/DominationPlayerState.h"

namespace
{
	// Standing-minus-crouched upper-body offsets measured at a clean crouch onset
	// (stationary, no montage). Shared by every character of the same class/weapon
	// so a weapon switched while already crouched still keeps its standing posture.
	struct FCrouchUpperOffset
	{
		FQuat Chest = FQuat::Identity;
		TArray<FQuat> Local;
	};

	FCriticalSection GCrouchUpperLock;
	TMap<FName, FCrouchUpperOffset> GCrouchUpperCache;

	// Corrected chain, parents before children. Arms are included because a crouch
	// idle can carry arms authored for a different torso: Vanguard keeps its bladed
	// shotgun torso with a pistol or knife, while the shared crouch idles expect a
	// square torso, so its arms turned 25-38 degrees to the right when crouching.
	const TCHAR* const GUpperBoneNames[] =
	{
		TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05"),
		TEXT("neck_01"), TEXT("neck_02"), TEXT("head"),
		TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
		TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r")
	};
}

FAnimNode_CrouchTransition::FAnimNode_CrouchTransition()
{
	Pelvis.BoneName = TEXT("pelvis");
	Spine.BoneName = TEXT("spine_01");
}

void FAnimNode_CrouchTransition::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);

	bInitializedState = false;
	bCaptureStart = false;
	bMatchReversalPose = false;
	ActiveClip = nullptr;
	LastPose.Reset();
	StartPose.Reset();
	StartSamplePose.Reset();
	CachedBoneValid.Reset();
	Elapsed = Duration = 0;
	MovementWeight = 1;

	bHaveStand = bStandClean = bEvalPrevCrouched = bUpperCorrection = false;
	CorrectionKey = StandKey = NAME_None;
}

void FAnimNode_CrouchTransition::PreUpdate(const UAnimInstance* Instance)
{
	bEligible = false;
	EnterClip = ExitClip = nullptr;

	const auto* Character = Cast<AShooterCharacter>(Instance->TryGetPawnOwner());
	const auto* Mesh = Instance->GetSkelMeshComponent();
	if (!Character || !Mesh || Mesh != Character->GetMesh())
	{
		return;
	}

	bCrouched = Character->bIsCrouched;
	const auto* Move = Character->GetCharacterMovement();
	Speed = Character->GetVelocity().Size2D();
	if (!bEnabled || !Move || !Move->IsMovingOnGround() || Mesh->IsSimulatingPhysics())
	{
		return;
	}

	EDominationCombatClass Class = EDominationCombatClass::Unselected;
	if (const auto* State = Character->GetPlayerState<ADominationPlayerState>())
	{
		Class = State->GetCombatClass();
	}
	if (Class == EDominationCombatClass::Unselected)
	{
		for (const auto& Def : GetDefault<UDominationClassSettings>()->CombatClasses)
		{
			if (Def.PawnClass.Get() && Character->IsA(Def.PawnClass.Get()))
			{
				Class = Def.CombatClass;
				break;
			}
		}
	}
	if (const auto* Clips = ClassClips.Find(Class))
	{
		EnterClip = Clips->Enter;
		ExitClip = Clips->Exit;
		bEligible = EnterClip && ExitClip;
	}

	// Upper-body offsets differ per class and weapon (knife turns ~30 degrees).
	const UCombatComponent* Combat = Character->GetCombatComponent();
	const UObject* Weapon = Combat ? Combat->GetCurrentWeapon() : nullptr;
	if (UpperKeyClass != int32(Class) || UpperKeyWeapon.Get() != Weapon)
	{
		UpperKeyClass = int32(Class);
		UpperKeyWeapon = Weapon;
		UpperKey = FName(*FString::Printf(TEXT("%d:%s"), UpperKeyClass, Weapon ? *Weapon->GetClass()->GetName() : TEXT("None")));
	}
	bUpperClean = Speed < 5.f && !Instance->IsAnyMontagePlaying();
}

void FAnimNode_CrouchTransition::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);

	const float Dt = FMath::Max(0.f, Context.GetDeltaTime());
	if (!bInitializedState || !bEligible)
	{
		// Spawn/reinitialize already crouched: show that pose without a fake entry.
		bInitializedState = true;
		bPreviousCrouched = bCrouched;
		ActiveClip = nullptr;
		Duration = 0;
		bCaptureStart = false;
		bMatchReversalPose = false;
		return;
	}

	if (bCrouched != bPreviousCrouched)
	{
		// The opposite clip is not a time inverse. Resolve its starting sample from
		// the last visible pose during evaluation, where compact bone data is valid.
		bMatchReversalPose = ActiveClip && Duration > 0;
		ActiveClip = bCrouched ? EnterClip : ExitClip;
		StartTime = 0.f;
		Duration = FMath::Max(.12f, ActiveClip->GetPlayLength() - StartTime);
		Elapsed = 0;
		bCaptureStart = true;
		bPreviousCrouched = bCrouched;
	}
	else
	{
		Elapsed += Dt;
	}

	// A stationary clip must not hold the feet while the player walks. Fade into
	// the already-running crouch walk blendspace without restarting its phase.
	MovementWeight = FMath::FInterpTo(MovementWeight, 1.f - FMath::Clamp(Speed / 120.f, 0.f, 1.f), Dt, 12.f);
	if (Elapsed >= Duration)
	{
		ActiveClip = nullptr;
		Duration = 0;
	}
}

void FAnimNode_CrouchTransition::InitializeBoneReferences(const FBoneContainer& Bones)
{
	// Compact-pose indices change with LOD. Preserve animation history by bone
	// name so a mid-transition LOD change cannot drop straight into its endpoint.
	TMap<FName, FTransform> OldLast, OldStart, OldSample;
	for (int32 I = 0; I < LowerNames.Num(); ++I)
	{
		if (LastPose.IsValidIndex(I)) OldLast.Add(LowerNames[I], LastPose[I]);
		if (StartPose.IsValidIndex(I)) OldStart.Add(LowerNames[I], StartPose[I]);
		if (StartSamplePose.IsValidIndex(I)) OldSample.Add(LowerNames[I], StartSamplePose[I]);
	}

	Pelvis.Initialize(Bones);
	Spine.Initialize(Bones);
	LowerBones.Reset();
	LowerNames.Reset();
	CachedBoneValid.Reset();
	LastPose.Reset();
	StartPose.Reset();
	StartSamplePose.Reset();

	UpperBones.Reset();
	for (const TCHAR* Name : GUpperBoneNames)
	{
		FBoneReference& UpperRef = UpperBones.AddDefaulted_GetRef();
		UpperRef.BoneName = Name;
		UpperRef.Initialize(Bones);
	}

	const auto& Ref = Bones.GetReferenceSkeleton();
	for (int32 I = 0; I < Bones.GetCompactPoseNumBones(); ++I)
	{
		const FCompactPoseBoneIndex Compact(I);
		int32 Index = Bones.MakeMeshPoseIndex(Compact).GetInt();
		bool Lower = Ref.GetBoneName(Index) == TEXT("pelvis");
		for (; Index != INDEX_NONE; Index = Ref.GetParentIndex(Index))
		{
			Lower |= Ref.GetBoneName(Index) == TEXT("thigh_l") || Ref.GetBoneName(Index) == TEXT("thigh_r");
		}
		if (!Lower)
		{
			continue;
		}

		const FName Name = Ref.GetBoneName(Bones.MakeMeshPoseIndex(Compact).GetInt());
		LowerBones.Add(Compact);
		LowerNames.Add(Name);

		const auto* Last = OldLast.Find(Name);
		const auto* Start = OldStart.Find(Name);
		const auto* Sample = OldSample.Find(Name);
		CachedBoneValid.Add(Last && Start && Sample);
		LastPose.Add(Last ? *Last : FTransform::Identity);
		StartPose.Add(Start ? *Start : FTransform::Identity);
		StartSamplePose.Add(Sample ? *Sample : FTransform::Identity);
	}
}

bool FAnimNode_CrouchTransition::IsValidToEvaluate(const USkeleton*, const FBoneContainer& Bones)
{
	return bEnabled && Pelvis.IsValidToEvaluate(Bones) && Spine.IsValidToEvaluate(Bones);
}

void FAnimNode_CrouchTransition::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& Out)
{
	const auto& Bones = Output.Pose.GetPose().GetBoneContainer();
	const auto Hip = Pelvis.GetCompactPoseIndex(Bones);
	const auto Chest = Spine.GetCompactPoseIndex(Bones);

	if (LastPose.Num() != LowerBones.Num() || StartSamplePose.Num() != LowerBones.Num() || CachedBoneValid.Num() != LowerBones.Num())
	{
		LastPose.SetNum(LowerBones.Num());
		StartPose.SetNum(LowerBones.Num());
		StartSamplePose.SetNum(LowerBones.Num());
		CachedBoneValid.Init(false, LowerBones.Num());
	}

	TArray<int32, TInlineAllocator<16>> MissingSampleBones;
	for (int32 I = 0; I < LowerBones.Num(); ++I)
	{
		if (CachedBoneValid[I])
		{
			continue;
		}
		const auto Bone = LowerBones[I];
		LastPose[I] = StartPose[I] = StartSamplePose[I] = Bone == Hip
			? Output.Pose.GetComponentSpaceTransform(Bone)
			: Output.Pose.GetLocalSpaceTransform(Bone);
		CachedBoneValid[I] = true;
		MissingSampleBones.Add(I);
	}

	// Upper-body input (before this node changes anything) for the standing-posture offset.
	const int32 NumUpper = UpperBones.Num();
	TArray<FCompactPoseBoneIndex, TInlineAllocator<8>> UpperIndex;
	TArray<FQuat, TInlineAllocator<8>> UpperLocal;
	for (int32 K = 0; K < NumUpper; ++K)
	{
		const bool bValid = UpperBones[K].IsValidToEvaluate(Bones);
		UpperIndex.Add(bValid ? UpperBones[K].GetCompactPoseIndex(Bones) : FCompactPoseBoneIndex(INDEX_NONE));
		UpperLocal.Add(bValid ? Output.Pose.GetLocalSpaceTransform(UpperIndex[K]).GetRotation() : FQuat::Identity);
	}

	const bool bStartingTransition = bCaptureStart;
	if (bCaptureStart)
	{
		StartPose = LastPose;
		bCaptureStart = false;
	}

	UpdateUpperCorrection(Output.Pose.GetComponentSpaceTransform(Chest).GetRotation(), UpperLocal, ActiveClip != nullptr && Duration > 0);

	FVector HipShift = FVector::ZeroVector;
	const bool bLowerTransition = bEligible && ActiveClip && Duration > 0 && StartPose.Num() == LowerBones.Num();
	if (!bLowerTransition)
	{
		for (int32 I = 0; I < LowerBones.Num(); ++I)
		{
			LastPose[I] = LowerBones[I] == Hip
				? Output.Pose.GetComponentSpaceTransform(Hip)
				: Output.Pose.GetLocalSpaceTransform(LowerBones[I]);
		}
	}
	else
	{
		FPoseContext Sample(Output.AnimInstanceProxy);
		FAnimationPoseData SampleData(Sample);

		if (bMatchReversalPose)
		{
			const float ClipLength = ActiveClip->GetPlayLength();
			StartTime = FPSCrouchReversal::FindPoseTime(ClipLength, [&](float Time)
			{
				ActiveClip->GetAnimationPose(SampleData, FAnimExtractContext(double(FMath::Clamp(Time, 0.f, ClipLength)), false));
				FCSPose<FCompactPose> CandidateCS;
				CandidateCS.InitPose(Sample.Pose);

				double Error = 0.0;
				int32 RotationCount = 0;
				double RotationError = 0.0;
				for (int32 I = 0; I < LowerBones.Num(); ++I)
				{
					const auto Bone = LowerBones[I];
					if (Bone == Hip)
					{
						// Component-space pelvis translation captures actual crouch depth.
						Error += FVector::DistSquared(StartPose[I].GetTranslation(), CandidateCS.GetComponentSpaceTransform(Hip).GetTranslation());
					}
					else
					{
						// Local leg rotations disambiguate poses at similar pelvis heights. Do
						// not score bone translations: the output preserves live limb lengths.
						const double Angle = StartPose[I].GetRotation().AngularDistance(Sample.Pose[Bone].GetRotation());
						RotationError += Angle * Angle;
						++RotationCount;
					}
				}
				// Normalize by bone count so LOD changes do not multiply leg influence.
				return Error + (RotationCount > 0 ? 100.0 * RotationError / RotationCount : 0.0);
			});
			Duration = FMath::Max(.12f, ClipLength - StartTime);
			Elapsed = 0.f;
			bMatchReversalPose = false;
		}

		if (bStartingTransition || !MissingSampleBones.IsEmpty())
		{
			ActiveClip->GetAnimationPose(SampleData, FAnimExtractContext(double(StartTime), false));
			FCSPose<FCompactPose> InitialCS;
			InitialCS.InitPose(Sample.Pose);
			for (int32 I = 0; I < LowerBones.Num(); ++I)
			{
				if (bStartingTransition || MissingSampleBones.Contains(I))
				{
					const auto Bone = LowerBones[I];
					StartSamplePose[I] = Bone == Hip ? InitialCS.GetComponentSpaceTransform(Bone) : Sample.Pose[Bone];
				}
			}
		}

		ActiveClip->GetAnimationPose(SampleData, FAnimExtractContext(double(FMath::Min(StartTime + Elapsed, ActiveClip->GetPlayLength())), false));
		FCSPose<FCompactPose> SampleCS;
		SampleCS.InitPose(Sample.Pose);

		const float In = FPSCrouchReversal::BlendIn(Elapsed, Duration);
		const float Tail = FPSCrouchReversal::BlendOut(Elapsed, Duration);
		const FTransform OriginalHip = Output.Pose.GetComponentSpaceTransform(Hip);
		FTransform NewHip = OriginalHip;
		TMap<int32, FTransform> Corrected;
		for (int32 I = 0; I < LowerBones.Num(); ++I)
		{
			const auto Bone = LowerBones[I];
			const FTransform Current = Bone == Hip ? OriginalHip : Output.Pose.GetLocalSpaceTransform(Bone);
			FTransform Animated = Bone == Hip ? SampleCS.GetComponentSpaceTransform(Bone) : Sample.Pose[Bone];
			Animated.SetTranslation(FPSCrouchReversal::BlendCapturedSample(StartPose[I].GetTranslation(), StartSamplePose[I].GetTranslation(), Animated.GetTranslation(), In));
			const FQuat RotationOffset = StartPose[I].GetRotation() * StartSamplePose[I].GetRotation().Inverse();
			Animated.SetRotation(FQuat::Slerp(RotationOffset, FQuat::Identity, In) * Animated.GetRotation());
			Animated.SetScale3D(FPSCrouchReversal::BlendCapturedSample(StartPose[I].GetScale3D(), StartSamplePose[I].GetScale3D(), Animated.GetScale3D(), In));

			// The first frame is the last visible pose, including rapid reversal. Moving
			// players then blend into the live walk pose; no movement input is blocked.
			const float Weight = FMath::Lerp(1.f, MovementWeight, In) * Tail;
			FTransform Result;
			Result.Blend(Current, Animated, Weight);
			Result.NormalizeRotation();
			if (Bone != Hip)
			{
				Result.SetTranslation(Current.GetTranslation());
				Result.SetScale3D(Current.GetScale3D());
			}
			LastPose[I] = Result;

			if (Bone == Hip)
			{
				NewHip = Result;
			}
			else
			{
				const auto Parent = Bones.GetParentBoneIndex(Bone);
				const auto* CorrectedParent = Corrected.Find(Parent.GetInt());
				Result = Result * (CorrectedParent ? *CorrectedParent : Output.Pose.GetComponentSpaceTransform(Parent));
			}
			Corrected.Add(Bone.GetInt(), Result);
			Out.Add(FBoneTransform(Bone, Result));
		}
		HipShift = NewHip.GetLocation() - OriginalHip.GetLocation();
	}

	if (!bLowerTransition && !bUpperCorrection)
	{
		return;
	}

	// Keep aim, shoulders and both hands as one rigid upper body. Translate them
	// with the new hip, without importing the source pistol arm pose or hip yaw.
	FTransform Upper = Output.Pose.GetComponentSpaceTransform(Chest);
	Upper.AddToTranslation(HipShift);
	if (bUpperCorrection)
	{
		Upper.SetRotation((ChestDelta * Upper.GetRotation()).GetNormalized());
	}
	Out.Add(FBoneTransform(Chest, Upper));

	if (bUpperCorrection)
	{
		// Re-apply the standing spine/neck/arm relation on top of the live crouched pose:
		// aim offset, ADS and montage motion remain, the crouch-only twist does not.
		TMap<int32, FTransform> UpperCS;
		UpperCS.Add(Chest.GetInt(), Upper);
		for (int32 K = 0; K < NumUpper; ++K)
		{
			const FCompactPoseBoneIndex Bone = UpperIndex[K];
			if (Bone.GetInt() == INDEX_NONE || !UpperDelta.IsValidIndex(K))
			{
				continue;
			}

			FTransform Local = Output.Pose.GetLocalSpaceTransform(Bone);
			Local.SetRotation((UpperDelta[K] * Local.GetRotation()).GetNormalized());

			// Every corrected bone must hang from an already corrected one; a stale
			// parent transform would detach it, so leave such a bone as animated.
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
			const FTransform* ParentCS = UpperCS.Find(Parent.GetInt());
			if (!ParentCS)
			{
				continue;
			}

			const FTransform Result = Local * (*ParentCS);
			UpperCS.Add(Bone.GetInt(), Result);
			Out.Add(FBoneTransform(Bone, Result));
		}
	}
	Out.Sort(FCompareBoneTransformIndex());
}

void FAnimNode_CrouchTransition::UpdateUpperCorrection(const FQuat& InputChest, const TArray<FQuat, TInlineAllocator<8>>& InputLocal, const bool bTransitionActive)
{
	const bool bCrouchActive = bPreserveStandingUpperBody && bEligible && bCrouched;
	if (!bCrouchActive)
	{
		// Latest standing sample; the crouch input switches within one update, so the
		// last standing frame and the first crouched frame share aim and ADS state.
		if (bEligible && !bCrouched)
		{
			StandChest = InputChest;
			StandUpperLocal.Reset();
			StandUpperLocal.Append(InputLocal);
			StandKey = UpperKey;
			bHaveStand = true;
			bStandClean = bUpperClean && !bTransitionActive;
		}
		bUpperCorrection = false;
		bEvalPrevCrouched = false;
		CorrectionKey = NAME_None;
		return;
	}

	const bool bOnset = !bEvalPrevCrouched;
	bEvalPrevCrouched = true;
	if (!bOnset && CorrectionKey == UpperKey)
	{
		return;
	}
	CorrectionKey = UpperKey;

	FCrouchUpperOffset Offset;
	bool bFound = false;
	if (bOnset && bHaveStand && StandKey == UpperKey && StandUpperLocal.Num() == InputLocal.Num())
	{
		Offset.Chest = (StandChest * InputChest.Inverse()).GetNormalized();
		for (int32 K = 0; K < InputLocal.Num(); ++K)
		{
			Offset.Local.Add((StandUpperLocal[K] * InputLocal[K].Inverse()).GetNormalized());
		}
		bFound = true;
	}

	{
		FScopeLock Lock(&GCrouchUpperLock);
		if (bFound && bStandClean && bUpperClean)
		{
			GCrouchUpperCache.Add(UpperKey, Offset);
		}
		else if (const FCrouchUpperOffset* Cached = GCrouchUpperCache.Find(UpperKey); Cached && Cached->Local.Num() == InputLocal.Num())
		{
			Offset = *Cached;
			bFound = true;
		}
	}

	bUpperCorrection = bFound;
	if (bFound)
	{
		ChestDelta = Offset.Chest;
		UpperDelta = Offset.Local;
	}
}
