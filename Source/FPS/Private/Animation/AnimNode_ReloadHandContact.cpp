#include "Animation/AnimNode_ReloadHandContact.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/ShooterAnimInstance.h"
#include "Customization/AppearanceGrip.h"
#include "Engine/SkeletalMesh.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapon/WeaponBase.h"
#include "TwoBoneIK.h"
#include "Tags/GameplayTags.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// The left arm of these reloads is baked into their animations (Saved/ClaudeProbe/bake37.py). With the
	// variable on (default), the node leaves the arm to the animation and only closes the fingers on the
	// magazine (their paths in the ABP still time that). 0 re-applies the contact IK on top, for comparison.
	TAutoConsoleVariable<int32> CVarReloadContactBaked(
		TEXT("a.ReloadHandContact.Baked"),
		1,
		TEXT("1: reloads whose left arm is baked into the animation are left to it (no contact IK)."));

	bool IsBakedReload(const UAnimMontage* Montage)
	{
		static const FName Baked[] = {TEXT("AM_Ranger_Sniper_Reload"), TEXT("AM_Trooper_AR_Reload")};
		if (!Montage || CVarReloadContactBaked.GetValueOnAnyThread() == 0)
		{
			return false;
		}
		const FName Name = Montage->GetFName();
		for (const FName& B : Baked)
		{
			if (Name == B)
			{
				return true;
			}
		}
		return false;
	}

	// The contact paths were measured on the first-person arms. Their hand bone is turned
	// 90 degrees about Z against the body skeleton's (fingers along Y there, along X here).
	const FQuat FirstPersonHandToBody(0.f, 0.f, 0.70710678f, 0.70710678f);

	// The shotgun path starts 2.8 cm inside the first-person hold. Weapon component space:
	// X + is the character's left, Y + is toward the muzzle, Z + is up (cm).
	const FVector ShotgunPathToHold(0, -0.900, -0.689);

	// DefaultSlot montages at most this long (fire recoil) keep the shotgun hold.
	constexpr float ShortActionLength = .75f;

	// Normalized reload time over which the shotgun path settles onto the hold.
	constexpr float SettleStart = .78f;
	constexpr float SettleEnd = .90f;

	// Distance (cm) of the first-person hand from its reload start grip over which a magazine weapon's
	// support hand leaves the idle hold for the magazine path (and returns to it at the end).
	constexpr float HoldLeaveStart = 1.5f;
	constexpr float HoldLeaveEnd = 8.f;

	// Distance (cm) behind the support hand, against its fingers, that the elbow is pulled toward.
	constexpr double GripPoleReach = 30.0;

	// Pistol reload: the first-person hand turns over by up to 180 degrees while it carries the magazine, which
	// the third-person arm cannot follow. The support hand instead holds the magazine upright where the
	// first-person hand carries it, and the animation instance shows that magazine in the hand.
	// Magazine bone origin in the first-person hand's frame while that hand carries it (cm).
	const FVector PistolMagazineInFirstPersonHand(5.232, 6.788, 2.601);
	// Magazine bone reference rotation in the pistol's component space (seated, upright).
	const FQuat PistolMagazineRest = FQuat(-0.56841, -0.420607, 0.56841, 0.420607).GetNormalized();

	// Curl (degrees, toward the palm) added to each finger joint while the support hand holds the magazine,
	// so the fingers close around it instead of keeping the authored open hand.
	// Primary weapon magazines are deeper than the pistol's: the fingers close less around them.
	constexpr float PrimaryFingerCurl = .7f;

	struct FFingerCurl { const TCHAR* Joints[3]; float Degrees[3]; };
	const FFingerCurl MagazineFingerCurl[] = {
		{{TEXT("index_01_l"), TEXT("index_02_l"), TEXT("index_03_l")}, {40.f, 45.f, 30.f}},
		{{TEXT("middle_01_l"), TEXT("middle_02_l"), TEXT("middle_03_l")}, {55.f, 60.f, 35.f}},
		{{TEXT("ring_01_l"), TEXT("ring_02_l"), TEXT("ring_03_l")}, {55.f, 60.f, 35.f}},
		{{TEXT("pinky_01_l"), TEXT("pinky_02_l"), TEXT("pinky_03_l")}, {55.f, 60.f, 35.f}},
		{{TEXT("thumb_01_l"), TEXT("thumb_02_l"), TEXT("thumb_03_l")}, {10.f, 15.f, 15.f}}};

	// Curls the left fingers toward the palm of the hand placed at HandCS (component space, not yet written to
	// the pose). Finger joints are rebuilt from their local transforms below that hand and appended to Out.
	void CurlFingers(FComponentSpacePoseContext& Output, FCompactPoseBoneIndex Hand, const FTransform& HandCS, float Weight, TArray<FBoneTransform>& Out)
	{
		const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
		auto Find = [&Bones](const TCHAR* Name)
		{
			const int32 Mesh = Bones.GetReferenceSkeleton().FindBoneIndex(FName(Name));
			return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
		};
		// Component space of a bone below the hand, after the hand moved (its own and its parents' local poses kept).
		auto BelowHand = [&](FCompactPoseBoneIndex Bone, FTransform& Result)
		{
			TArray<FCompactPoseBoneIndex, TInlineAllocator<8>> Chain;
			for (FCompactPoseBoneIndex I = Bone; I != Hand; I = Bones.GetParentBoneIndex(I))
			{
				if (!I.IsValid() || Chain.Num() >= 8)
				{
					return false;
				}
				Chain.Add(I);
			}
			Result = HandCS;
			for (int32 K = Chain.Num() - 1; K >= 0; --K)
			{
				Result = Output.Pose.GetLocalSpaceTransform(Chain[K]) * Result;
			}
			return true;
		};

		FTransform Index, Middle, Pinky;
		if (!BelowHand(Find(TEXT("index_01_l")), Index) || !BelowHand(Find(TEXT("middle_01_l")), Middle) || !BelowHand(Find(TEXT("pinky_01_l")), Pinky))
		{
			return;
		}
		// Left palm normal: fingers x (index - pinky), negated for the left hand.
		const FVector Palm = -FVector::CrossProduct(Middle.GetLocation() - HandCS.GetLocation(), Index.GetLocation() - Pinky.GetLocation()).GetSafeNormal();
		if (Palm.IsNearlyZero())
		{
			return;
		}

		for (const FFingerCurl& Finger : MagazineFingerCurl)
		{
			const FCompactPoseBoneIndex Joint[3]
				= {Find(Finger.Joints[0]), Find(Finger.Joints[1]), Find(Finger.Joints[2])};
			FTransform Parent;
			if (!Joint[0].IsValid() || !Joint[1].IsValid() || !Joint[2].IsValid()
				|| Bones.GetParentBoneIndex(Joint[1]) != Joint[0] || Bones.GetParentBoneIndex(Joint[2]) != Joint[1]
				|| !BelowHand(Bones.GetParentBoneIndex(Joint[0]), Parent))
			{
				continue;
			}
			FVector Along = FVector::ZeroVector;
			for (int32 K = 0; K < 3; ++K)
			{
				FTransform J = Output.Pose.GetLocalSpaceTransform(Joint[K]) * Parent;
				// Direction of this segment: toward the next joint, or along the previous segment at the tip.
				if (K < 2)
				{
					Along = (Output.Pose.GetLocalSpaceTransform(Joint[K + 1]) * J).GetLocation() - J.GetLocation();
				}
				const FVector Axis = FVector::CrossProduct(Along, Palm).GetSafeNormal();
				if (!Axis.IsNearlyZero())
				{
					const FQuat Curl(Axis, FMath::DegreesToRadians(Finger.Degrees[K] * Weight));
					J.SetRotation((Curl * J.GetRotation()).GetNormalized());
					if (K < 2)
					{
						Along = Curl.RotateVector(Along);
					}
				}
				Out.Add(FBoneTransform(Joint[K], J));
				Parent = J;
			}
		}
		Out.Sort([](const FBoneTransform& A, const FBoneTransform& B) { return A.BoneIndex < B.BoneIndex; });
	}
}

FTransform ReloadHandContact::PistolHandInMagazine()
{
	// Palm against the magazine's side, fingers around its front, wrist below its middle.
	return FTransform(FQuat(0.70710678, 0., 0., -0.70710678), FVector(-7., -1.5, -3.9));
}

FAnimNode_ReloadHandContact::FAnimNode_ReloadHandContact()
{
	Upper.BoneName = TEXT("upperarm_l");
	Lower.BoneName = TEXT("lowerarm_l");
	Hand.BoneName = TEXT("hand_l");
	Grip.BoneName = TEXT("hand_r");
}

void FAnimNode_ReloadHandContact::PreUpdate(const UAnimInstance* Instance)
{
	bAttachmentReady = false;

	const auto* Shooter = Cast<UShooterAnimInstance>(Instance);
	Playback = Shooter ? &Shooter->GetContactPlayback() : nullptr;

	const auto* C = Cast<AShooterCharacter>(Instance->TryGetPawnOwner());
	const auto* Body = Instance->GetSkelMeshComponent();
	if (!bEnabled || !C || Body != C->GetMesh() || Body->IsSimulatingPhysics() || !C->GetCombatComponent())
	{
		return;
	}

	const auto* Weapon = C->GetCombatComponent()->GetCurrentWeapon();
	const auto* Gun = Weapon ? Weapon->GetMesh3P() : nullptr;
	const auto* Parent = Gun ? Cast<USkeletalMeshComponent>(Gun->GetAttachParent()) : nullptr;
	if (!Parent || !Parent->DoesSocketExist(Grip.BoneName) || !Body->GetSkeletalMeshAsset() || !Parent->GetSkeletalMeshAsset())
	{
		return;
	}

	// Express attachment within its own wrist, not visible-body world space minus
	// driver wrist world space. AppearancePose applies that body displacement later.
	WeaponInParentGrip = Gun->GetRelativeTransform()
		* Parent->GetSocketTransform(Gun->GetAttachSocketName()).GetRelativeTransform(Parent->GetSocketTransform(Grip.BoneName));

	RightBasis = LeftBasis = FTransform::Identity;
	if (Parent != Body)
	{
		const auto& Source = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		const auto& Target = Parent->GetSkeletalMeshAsset()->GetRefSkeleton();
		if (!AppearanceGrip::BuildHandBasis(Source, Target, Grip.BoneName, RightBasis)
			|| !AppearanceGrip::BuildHandBasis(Source, Target, Hand.BoneName, LeftBasis))
		{
			return;
		}
	}

	CachedWeaponTag = Weapon->GetWeaponTag();
	bAttachmentReady = true;
}

void FAnimNode_ReloadHandContact::InitializeBoneReferences(const FBoneContainer& Bones)
{
	Upper.Initialize(Bones);
	Lower.Initialize(Bones);
	Hand.Initialize(Bones);
	Grip.Initialize(Bones);
}

bool FAnimNode_ReloadHandContact::IsValidToEvaluate(const USkeleton*, const FBoneContainer& Bones)
{
	return bEnabled
		&& Upper.IsValidToEvaluate(Bones)
		&& Lower.IsValidToEvaluate(Bones)
		&& Hand.IsValidToEvaluate(Bones)
		&& Grip.IsValidToEvaluate(Bones);
}

void FAnimNode_ReloadHandContact::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& Out)
{
	if (!bAttachmentReady || !Playback)
	{
		return;
	}

	// Shotgun support hand: outside full-weight DefaultSlot actions it rests on the forend
	// with the first-person hold, at the grip the reload contact path starts and ends on.
	// Idle, a finished draw and a finished reload then share one hold, instead of the open
	// palm under the receiver that the generic support offset gives this weapon.
	const bool bRestGrip = CachedWeaponTag.MatchesTagExact(RogueTags::Weapon_Type_Shotgun);

	// Rifle and sniper: keep the first-person grip exactly (converted to the body's hand axes), since the
	// weapon's own reload animation moves the magazine with that hand. The first-person arm reaches it with
	// the elbow on the fingers' far side; the elbow is swung toward that side below (pole) so the wrist stays
	// straight, instead of keeping the authored elbow and folding the wrist back.
	const bool bFirstPersonGrip = CachedWeaponTag.MatchesTagExact(RogueTags::Weapon_Type_Rifle)
		|| CachedWeaponTag.MatchesTagExact(RogueTags::Weapon_Type_Sniper);
	// Pistol: hold the magazine upright where the first-person hand carries it, with the authored elbow.
	const bool bMagazineGrip = CachedWeaponTag.MatchesTagExact(RogueTags::Weapon_Type_Pistol);
	const bool bHoldsIdle = bFirstPersonGrip || bMagazineGrip;

	// Path sample (third-person weapon component space) -> target in the driver's right hand space.
	auto ToGrip = [this, bRestGrip, bFirstPersonGrip, bMagazineGrip](FTransform Sample)
	{
		if (bRestGrip)
		{
			Sample.SetRotation((Sample.GetRotation() * FirstPersonHandToBody).GetNormalized());
			Sample.AddToTranslation(ShotgunPathToHold);
		}
		else if (bFirstPersonGrip)
		{
			Sample.SetRotation((Sample.GetRotation() * FirstPersonHandToBody).GetNormalized());
		}
		else if (bMagazineGrip)
		{
			const FVector Magazine = Sample.TransformPosition(PistolMagazineInFirstPersonHand);
			Sample = ReloadHandContact::PistolHandInMagazine() * FTransform(PistolMagazineRest, Magazine);
		}
		FTransform Target = Sample * WeaponInParentGrip;
		// Invert AppearanceGrip::MapRelativeHand, including the left palm axes.
		Target.SetRotation((Target.GetRotation() * LeftBasis.GetRotation()).GetNormalized());
		return Target * RightBasis.Inverse();
	};

	float ContactWeight = 0.f;
	float SlotWeight = 0.f;
	float ActionWeight = 0.f;
	float PoleWeight = 0.f;
	float GripWeight = 0.f;
	float CurlWeight = 0.f;
	FTransform TargetInGrip = FTransform::Identity;
	const FReloadHandPath* RestPath = nullptr;

	// NativeUpdate's snapshot is captured AFTER this frame's montage advance. PreUpdate
	// runs before it and would leave the hand one frame behind the magazine.
	for (const auto& Action : *Playback)
	{
		const auto* Montage = Action.Montage;
		if (Montage && Montage->GetPlayLength() > ShortActionLength)
		{
			ActionWeight += FMath::Max(Action.Weight, 0.f);
		}

		// Short recoil montages (fire) keep the hold; longer DefaultSlot actions (draw,
		// inspect, throws) move the support hand themselves and release it meanwhile.
		if (bRestGrip && Montage && Montage->GetPlayLength() > ShortActionLength && Montage->IsValidSlot(TEXT("DefaultSlot")))
		{
			SlotWeight += FMath::Max(Action.Weight, 0.f);
		}

		const auto* Path = Paths.Find(Montage);
		const float Weight = Action.Weight;
		if (!Path || Path->WeaponTag != CachedWeaponTag || Path->Samples.Num() < 2)
		{
			continue;
		}

		const double Frame = FMath::Clamp(Action.NormalizedTime, 0.f, 1.f) * (Path->Samples.Num() - 1);
		const int32 I = FMath::FloorToInt(Frame);
		const int32 J = FMath::Min(I + 1, Path->Samples.Num() - 1);
		FTransform Sample;
		Sample.Blend(Path->Samples[I], Path->Samples[J], Frame - I);

		// The first-person hand starts and ends the reload on the grip; away from it, it handles the magazine.
		const float Leave = static_cast<float>(FVector::Dist(Sample.GetTranslation(), Path->Samples[0].GetTranslation()));
		const float Away = FMath::SmoothStep(HoldLeaveStart, HoldLeaveEnd, Leave);
		if (!bMagazineGrip)
		{
			const float Recover = bRestGrip ? 1.f : 1.f - FMath::SmoothStep(.80f, .98f, Action.NormalizedTime);
			CurlWeight = FMath::Max(CurlWeight, FMath::Clamp(Weight, 0.f, 1.f) * Away * Recover * PrimaryFingerCurl);
		}
		if (IsBakedReload(Montage))
		{
			continue;
		}
		RestPath = Path;
		if (Weight <= ContactWeight)
		{
			continue;
		}
		TargetInGrip = ToGrip(Sample);

		if (bFirstPersonGrip)
		{
			PoleWeight = 1.f;
		}
		GripWeight = bMagazineGrip ? 1.f : 0.f;
		if (bHoldsIdle && bHaveIdleHold && IdleHoldTag == CachedWeaponTag)
		{
			// On the grip, keep the idle hold instead of the turned-over magazine grip, and blend between them as it leaves.
			FTransform Held;
			Held.Blend(IdleHoldInGrip, TargetInGrip, Away);
			TargetInGrip = Held;
			PoleWeight = bFirstPersonGrip ? Away : 0.f;
			GripWeight = bMagazineGrip ? Away : 0.f;
		}

		if (bRestGrip)
		{
			// Settle onto the hold before the montage blends out, so the rest grip takes over without a slide.
			const float Home = FMath::SmoothStep(SettleStart, SettleEnd, Action.NormalizedTime);
			if (Home > 0.f)
			{
				FTransform Settled;
				Settled.Blend(TargetInGrip, ToGrip(Path->Samples[0]), Home);
				TargetInGrip = Settled;
			}
		}

		// Magazine handling has finished. Return over the authored recovery tail,
		// rather than dropping the contact constraint in the short montage blend-out.
		// A rest grip weapon keeps following the path: it ends on that grip.
		const float Tail = bRestGrip ? 1.f : 1.f - FMath::SmoothStep(.80f, .98f, Action.NormalizedTime);
		ContactWeight = FMath::Clamp(Weight, 0.f, 1.f) * Tail;
	}

	if (bRestGrip)
	{
		if (!RestPath)
		{
			for (const auto& Pair : Paths)
			{
				if (Pair.Value.WeaponTag == CachedWeaponTag && Pair.Value.Samples.Num() >= 2)
				{
					RestPath = &Pair.Value;
					break;
				}
			}
		}

		const float RestWeight = 1.f - FMath::Clamp(SlotWeight, 0.f, 1.f);
		if (RestPath && RestWeight > ZERO_ANIMWEIGHT_THRESH)
		{
			const FTransform Rest = ToGrip(RestPath->Samples[0]);
			if (ContactWeight > ZERO_ANIMWEIGHT_THRESH)
			{
				const FTransform Contact = TargetInGrip;
				TargetInGrip.Blend(Rest, Contact, ContactWeight / (ContactWeight + RestWeight));
			}
			else
			{
				TargetInGrip = Rest;
			}
			ContactWeight = FMath::Min(1.f, ContactWeight + RestWeight);
		}
	}

	const auto& Bones = Output.Pose.GetPose().GetBoneContainer();
	const auto U = Upper.GetCompactPoseIndex(Bones);
	const auto L = Lower.GetCompactPoseIndex(Bones);
	const auto H = Hand.GetCompactPoseIndex(Bones);
	const auto R = Grip.GetCompactPoseIndex(Bones);

	// Remember the idle hold (support grip IK applied, no action playing) in the right hand's space.
	if (bHoldsIdle && ActionWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		IdleHoldInGrip = Output.Pose.GetComponentSpaceTransform(H).GetRelativeTransform(Output.Pose.GetComponentSpaceTransform(R));
		IdleHoldTag = CachedWeaponTag;
		bHaveIdleHold = true;
	}

	if (ContactWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		// Baked reload: the animation places the arm; only close the fingers on the magazine.
		if (CurlWeight > ZERO_ANIMWEIGHT_THRESH)
		{
			CurlFingers(Output, H, Output.Pose.GetComponentSpaceTransform(H), CurlWeight, Out);
		}
		return;
	}

	FTransform A = Output.Pose.GetComponentSpaceTransform(U);
	FTransform B = Output.Pose.GetComponentSpaceTransform(L);
	FTransform C = Output.Pose.GetComponentSpaceTransform(H);
	const FTransform Target = TargetInGrip * Output.Pose.GetComponentSpaceTransform(R);
	const FVector Goal = FMath::Lerp(C.GetLocation(), Target.GetLocation(), ContactWeight);
	const FQuat Rotation = FQuat::Slerp(C.GetRotation(), Target.GetRotation(), ContactWeight).GetNormalized();

	// Preserve the authored elbow plane and both segment lengths. Never stretch. Away from the grip a
	// magazine weapon's support hand pulls the elbow plane toward the side its fingers point away from (straight wrist).
	FVector JointTarget = B.GetLocation();
	if (PoleWeight > 0.f)
	{
		const FVector Straight = Goal - Target.GetRotation().GetAxisX() * GripPoleReach;
		JointTarget = FMath::Lerp(JointTarget, Straight, PoleWeight * ContactWeight);
	}
	AnimationCore::SolveTwoBoneIK(A, B, C, JointTarget, Goal, false, 1., 1.);
	C.SetRotation(Rotation);

	Out.Add(FBoneTransform(U, A));
	Out.Add(FBoneTransform(L, B));
	Out.Add(FBoneTransform(H, C));
	if (bMagazineGrip)
	{
		CurlWeight = GripWeight * ContactWeight;
	}
	if (CurlWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		CurlFingers(Output, H, C, CurlWeight, Out);
	}
}
