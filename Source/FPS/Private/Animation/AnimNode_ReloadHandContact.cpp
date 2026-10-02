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

FAnimNode_ReloadHandContact::FAnimNode_ReloadHandContact()
{
 Upper.BoneName=TEXT("upperarm_l");Lower.BoneName=TEXT("lowerarm_l");Hand.BoneName=TEXT("hand_l");Grip.BoneName=TEXT("hand_r");
}
void FAnimNode_ReloadHandContact::PreUpdate(const UAnimInstance* Instance)
{
 bAttachmentReady=false;
 const auto* Shooter=Cast<UShooterAnimInstance>(Instance);
 Playback=Shooter?&Shooter->GetContactPlayback():nullptr;
 const auto* C=Cast<AShooterCharacter>(Instance->TryGetPawnOwner());
 const auto* Body=Instance->GetSkelMeshComponent();
 if(!bEnabled||!C||Body!=C->GetMesh()||Body->IsSimulatingPhysics()||!C->GetCombatComponent())return;
 const auto* Weapon=C->GetCombatComponent()->GetCurrentWeapon();
 const auto* Gun=Weapon?Weapon->GetMesh3P():nullptr;
 const auto* Parent=Gun?Cast<USkeletalMeshComponent>(Gun->GetAttachParent()):nullptr;
 if(!Parent||!Parent->DoesSocketExist(Grip.BoneName)||!Body->GetSkeletalMeshAsset()||!Parent->GetSkeletalMeshAsset())return;
 // Express attachment within its own wrist, not visible-body world space minus
 // driver wrist world space. AppearancePose applies that body displacement later.
 WeaponInParentGrip=Gun->GetRelativeTransform()*Parent->GetSocketTransform(Gun->GetAttachSocketName()).GetRelativeTransform(Parent->GetSocketTransform(Grip.BoneName));
 RightBasis=LeftBasis=FTransform::Identity;
 if(Parent!=Body)
 {
  const auto& Source=Body->GetSkeletalMeshAsset()->GetRefSkeleton();
  const auto& Target=Parent->GetSkeletalMeshAsset()->GetRefSkeleton();
  if(!AppearanceGrip::BuildHandBasis(Source,Target,Grip.BoneName,RightBasis)||!AppearanceGrip::BuildHandBasis(Source,Target,Hand.BoneName,LeftBasis))return;
 }
 CachedWeaponTag=Weapon->GetWeaponTag();
 bAttachmentReady=true;
}
void FAnimNode_ReloadHandContact::InitializeBoneReferences(const FBoneContainer& Bones)
{
 Upper.Initialize(Bones);Lower.Initialize(Bones);Hand.Initialize(Bones);Grip.Initialize(Bones);
}
bool FAnimNode_ReloadHandContact::IsValidToEvaluate(const USkeleton*,const FBoneContainer& Bones)
{
 return bEnabled&&Upper.IsValidToEvaluate(Bones)&&Lower.IsValidToEvaluate(Bones)&&Hand.IsValidToEvaluate(Bones)&&Grip.IsValidToEvaluate(Bones);
}
void FAnimNode_ReloadHandContact::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out)
{
 if(!bAttachmentReady||!Playback)return;
 float ContactWeight=0.f;
 FTransform TargetInGrip=FTransform::Identity;
 // NativeUpdate's snapshot is captured AFTER this frame's montage advance. PreUpdate
 // runs before it and would leave the hand one frame behind the magazine.
 for(const auto& Action:*Playback)
 {
  const auto* Montage=Action.Montage;
  const auto* Path=Paths.Find(Montage);
  const float Weight=Action.Weight;
  if(!Path||Path->WeaponTag!=CachedWeaponTag||Path->Samples.Num()<2||Weight<=ContactWeight)continue;
  const double Frame=FMath::Clamp(Action.NormalizedTime,0.f,1.f)*(Path->Samples.Num()-1);
  const int32 I=FMath::FloorToInt(Frame),J=FMath::Min(I+1,Path->Samples.Num()-1);
  FTransform Sample;Sample.Blend(Path->Samples[I],Path->Samples[J],Frame-I);
  TargetInGrip=Sample*WeaponInParentGrip;
  // Invert AppearanceGrip::MapRelativeHand, including the left palm axes.
  TargetInGrip.SetRotation((TargetInGrip.GetRotation()*LeftBasis.GetRotation()).GetNormalized());
  TargetInGrip=TargetInGrip*RightBasis.Inverse();
  // Magazine handling has finished. Return over the authored recovery tail,
  // rather than dropping the contact constraint in the short montage blend-out.
  ContactWeight=FMath::Clamp(Weight,0.f,1.f)*(1.f-FMath::SmoothStep(.80f,.98f,Action.NormalizedTime));
 }
 if(ContactWeight<=ZERO_ANIMWEIGHT_THRESH)return;
 const auto& Bones=Output.Pose.GetPose().GetBoneContainer();
 const auto U=Upper.GetCompactPoseIndex(Bones),L=Lower.GetCompactPoseIndex(Bones),H=Hand.GetCompactPoseIndex(Bones),R=Grip.GetCompactPoseIndex(Bones);
 FTransform A=Output.Pose.GetComponentSpaceTransform(U),B=Output.Pose.GetComponentSpaceTransform(L),C=Output.Pose.GetComponentSpaceTransform(H);
 const FTransform Target=TargetInGrip*Output.Pose.GetComponentSpaceTransform(R);
 const FVector Goal=FMath::Lerp(C.GetLocation(),Target.GetLocation(),ContactWeight);
 const FQuat Rotation=FQuat::Slerp(C.GetRotation(),Target.GetRotation(),ContactWeight).GetNormalized();
 // Preserve the authored elbow plane and both segment lengths. Never stretch.
 AnimationCore::SolveTwoBoneIK(A,B,C,B.GetLocation(),Goal,false,1.,1.);
 C.SetRotation(Rotation);
 Out.Add(FBoneTransform(U,A));Out.Add(FBoneTransform(L,B));Out.Add(FBoneTransform(H,C));
}
