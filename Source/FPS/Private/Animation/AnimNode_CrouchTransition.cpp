#include "Animation/AnimNode_CrouchTransition.h"
#include "CrouchReversalSampling.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Character/ShooterCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Player/DominationPlayerState.h"

FAnimNode_CrouchTransition::FAnimNode_CrouchTransition()
{
 Pelvis.BoneName=TEXT("pelvis");Spine.BoneName=TEXT("spine_01");
}
void FAnimNode_CrouchTransition::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
 FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
 bInitializedState=false;bCaptureStart=false;bMatchReversalPose=false;ActiveClip=nullptr;LastPose.Reset();StartPose.Reset();StartSamplePose.Reset();CachedBoneValid.Reset();Elapsed=Duration=0;MovementWeight=1;
}
void FAnimNode_CrouchTransition::PreUpdate(const UAnimInstance* Instance)
{
 bEligible=false;EnterClip=ExitClip=nullptr;
 const auto* Character=Cast<AShooterCharacter>(Instance->TryGetPawnOwner());
 const auto* Mesh=Instance->GetSkelMeshComponent();
 if(!Character||!Mesh||Mesh!=Character->GetMesh())return;
 bCrouched=Character->bIsCrouched;
 const auto* Move=Character->GetCharacterMovement();
 Speed=Character->GetVelocity().Size2D();
 if(!bEnabled||!Move||!Move->IsMovingOnGround()||Mesh->IsSimulatingPhysics())return;
 EDominationCombatClass Class=EDominationCombatClass::Unselected;
 if(const auto* State=Character->GetPlayerState<ADominationPlayerState>())Class=State->GetCombatClass();
 if(Class==EDominationCombatClass::Unselected)
  for(const auto& Def:GetDefault<UDominationClassSettings>()->CombatClasses)
   if(Def.PawnClass.Get()&&Character->IsA(Def.PawnClass.Get())){Class=Def.CombatClass;break;}
 if(const auto* Clips=ClassClips.Find(Class)){EnterClip=Clips->Enter;ExitClip=Clips->Exit;bEligible=EnterClip&&ExitClip;}
}
void FAnimNode_CrouchTransition::UpdateInternal(const FAnimationUpdateContext& Context)
{
 FAnimNode_SkeletalControlBase::UpdateInternal(Context);
 const float Dt=FMath::Max(0.f,Context.GetDeltaTime());
 if(!bInitializedState||!bEligible)
 {
  // Spawn/reinitialize already crouched: show that pose without a fake entry.
  bInitializedState=true;bPreviousCrouched=bCrouched;ActiveClip=nullptr;Duration=0;bCaptureStart=false;bMatchReversalPose=false;return;
 }
 if(bCrouched!=bPreviousCrouched)
 {
  // The opposite clip is not a time inverse. Resolve its starting sample from
  // the last visible pose during evaluation, where compact bone data is valid.
  bMatchReversalPose=ActiveClip&&Duration>0;
  ActiveClip=bCrouched?EnterClip:ExitClip;
  StartTime=0.f;
  Duration=FMath::Max(.12f,ActiveClip->GetPlayLength()-StartTime);
  Elapsed=0;bCaptureStart=true;bPreviousCrouched=bCrouched;
 }
 else Elapsed+=Dt;
 // A stationary clip must not hold the feet while the player walks. Fade into
 // the already-running crouch walk blendspace without restarting its phase.
 MovementWeight=FMath::FInterpTo(MovementWeight,1.f-FMath::Clamp(Speed/120.f,0.f,1.f),Dt,12.f);
 if(Elapsed>=Duration){ActiveClip=nullptr;Duration=0;}
}
void FAnimNode_CrouchTransition::InitializeBoneReferences(const FBoneContainer& Bones)
{
 // Compact-pose indices change with LOD. Preserve animation history by bone
 // name so a mid-transition LOD change cannot drop straight into its endpoint.
 TMap<FName,FTransform> OldLast,OldStart,OldSample;
 for(int32 I=0;I<LowerNames.Num();++I)
 {
  if(LastPose.IsValidIndex(I))OldLast.Add(LowerNames[I],LastPose[I]);
  if(StartPose.IsValidIndex(I))OldStart.Add(LowerNames[I],StartPose[I]);
  if(StartSamplePose.IsValidIndex(I))OldSample.Add(LowerNames[I],StartSamplePose[I]);
 }
 Pelvis.Initialize(Bones);Spine.Initialize(Bones);LowerBones.Reset();LowerNames.Reset();CachedBoneValid.Reset();LastPose.Reset();StartPose.Reset();StartSamplePose.Reset();
 const auto& Ref=Bones.GetReferenceSkeleton();
 for(int32 I=0;I<Bones.GetCompactPoseNumBones();++I)
 {
  const FCompactPoseBoneIndex Compact(I);
  int32 Index=Bones.MakeMeshPoseIndex(Compact).GetInt();
  bool Lower=Ref.GetBoneName(Index)==TEXT("pelvis");
  for(;Index!=INDEX_NONE;Index=Ref.GetParentIndex(Index))
   Lower|=Ref.GetBoneName(Index)==TEXT("thigh_l")||Ref.GetBoneName(Index)==TEXT("thigh_r");
  if(Lower)
  {
   const FName Name=Ref.GetBoneName(Bones.MakeMeshPoseIndex(Compact).GetInt());
   LowerBones.Add(Compact);LowerNames.Add(Name);
   const auto* Last=OldLast.Find(Name);const auto* Start=OldStart.Find(Name);const auto* Sample=OldSample.Find(Name);
   CachedBoneValid.Add(Last&&Start&&Sample);LastPose.Add(Last?*Last:FTransform::Identity);StartPose.Add(Start?*Start:FTransform::Identity);StartSamplePose.Add(Sample?*Sample:FTransform::Identity);
  }
 }
}
bool FAnimNode_CrouchTransition::IsValidToEvaluate(const USkeleton*,const FBoneContainer& Bones)
{
 return bEnabled&&Pelvis.IsValidToEvaluate(Bones)&&Spine.IsValidToEvaluate(Bones);
}
void FAnimNode_CrouchTransition::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out)
{
 const auto& Bones=Output.Pose.GetPose().GetBoneContainer();
 const auto Hip=Pelvis.GetCompactPoseIndex(Bones),Chest=Spine.GetCompactPoseIndex(Bones);
 if(LastPose.Num()!=LowerBones.Num()||StartSamplePose.Num()!=LowerBones.Num()||CachedBoneValid.Num()!=LowerBones.Num())
 {
  LastPose.SetNum(LowerBones.Num());StartPose.SetNum(LowerBones.Num());StartSamplePose.SetNum(LowerBones.Num());CachedBoneValid.Init(false,LowerBones.Num());
 }
 TArray<int32,TInlineAllocator<16>> MissingSampleBones;
 for(int32 I=0;I<LowerBones.Num();++I)if(!CachedBoneValid[I])
 {
  const auto Bone=LowerBones[I];LastPose[I]=StartPose[I]=StartSamplePose[I]=Bone==Hip?Output.Pose.GetComponentSpaceTransform(Bone):Output.Pose.GetLocalSpaceTransform(Bone);CachedBoneValid[I]=true;MissingSampleBones.Add(I);
 }
 const bool bStartingTransition=bCaptureStart;
 if(bCaptureStart){StartPose=LastPose;bCaptureStart=false;}
 if(!bEligible||!ActiveClip||Duration<=0||StartPose.Num()!=LowerBones.Num())
 {
  for(int32 I=0;I<LowerBones.Num();++I)LastPose[I]=LowerBones[I]==Hip?Output.Pose.GetComponentSpaceTransform(Hip):Output.Pose.GetLocalSpaceTransform(LowerBones[I]);
  return;
 }
 FPoseContext Sample(Output.AnimInstanceProxy);
 FAnimationPoseData SampleData(Sample);
 if(bMatchReversalPose)
 {
  const float ClipLength=ActiveClip->GetPlayLength();
  StartTime=FPSCrouchReversal::FindPoseTime(ClipLength,[&](float Time)
  {
   ActiveClip->GetAnimationPose(SampleData,FAnimExtractContext(double(FMath::Clamp(Time,0.f,ClipLength)),false));
   FCSPose<FCompactPose> CandidateCS;CandidateCS.InitPose(Sample.Pose);
   double Error=0.0;
   int32 RotationCount=0;
   double RotationError=0.0;
   for(int32 I=0;I<LowerBones.Num();++I)
   {
    const auto Bone=LowerBones[I];
    if(Bone==Hip)
    {
     // Component-space pelvis translation captures actual crouch depth.
     Error+=FVector::DistSquared(StartPose[I].GetTranslation(),CandidateCS.GetComponentSpaceTransform(Hip).GetTranslation());
    }
    else
    {
     // Local leg rotations disambiguate poses at similar pelvis heights. Do
     // not score bone translations: the output preserves live limb lengths.
     const double Angle=StartPose[I].GetRotation().AngularDistance(Sample.Pose[Bone].GetRotation());
     RotationError+=Angle*Angle;
     ++RotationCount;
    }
   }
   // Normalize by bone count so LOD changes do not multiply leg influence.
   return Error+(RotationCount>0?100.0*RotationError/RotationCount:0.0);
  });
  Duration=FMath::Max(.12f,ClipLength-StartTime);
  Elapsed=0.f;bMatchReversalPose=false;
 }
 if(bStartingTransition||!MissingSampleBones.IsEmpty())
 {
  ActiveClip->GetAnimationPose(SampleData,FAnimExtractContext(double(StartTime),false));
  FCSPose<FCompactPose> InitialCS;InitialCS.InitPose(Sample.Pose);
  for(int32 I=0;I<LowerBones.Num();++I)if(bStartingTransition||MissingSampleBones.Contains(I))
  {
   const auto Bone=LowerBones[I];
   StartSamplePose[I]=Bone==Hip?InitialCS.GetComponentSpaceTransform(Bone):Sample.Pose[Bone];
  }
 }
 ActiveClip->GetAnimationPose(SampleData,FAnimExtractContext(double(FMath::Min(StartTime+Elapsed,ActiveClip->GetPlayLength())),false));
 FCSPose<FCompactPose> SampleCS;SampleCS.InitPose(Sample.Pose);
 const float In=FPSCrouchReversal::BlendIn(Elapsed,Duration);
 const float Tail=FPSCrouchReversal::BlendOut(Elapsed,Duration);
 const FTransform OriginalHip=Output.Pose.GetComponentSpaceTransform(Hip);
 FTransform NewHip=OriginalHip;
 TMap<int32,FTransform> Corrected;
 for(int32 I=0;I<LowerBones.Num();++I)
 {
  const auto Bone=LowerBones[I];const FTransform Current=Bone==Hip?OriginalHip:Output.Pose.GetLocalSpaceTransform(Bone);
  FTransform Animated=Bone==Hip?SampleCS.GetComponentSpaceTransform(Bone):Sample.Pose[Bone];
  Animated.SetTranslation(FPSCrouchReversal::BlendCapturedSample(StartPose[I].GetTranslation(),StartSamplePose[I].GetTranslation(),Animated.GetTranslation(),In));
  const FQuat RotationOffset=StartPose[I].GetRotation()*StartSamplePose[I].GetRotation().Inverse();
  Animated.SetRotation(FQuat::Slerp(RotationOffset,FQuat::Identity,In)*Animated.GetRotation());
  Animated.SetScale3D(FPSCrouchReversal::BlendCapturedSample(StartPose[I].GetScale3D(),StartSamplePose[I].GetScale3D(),Animated.GetScale3D(),In));
  // The first frame is the last visible pose, including rapid reversal. Moving
  // players then blend into the live walk pose; no movement input is blocked.
  const float Weight=FMath::Lerp(1.f,MovementWeight,In)*Tail;
  FTransform Result;Result.Blend(Current,Animated,Weight);Result.NormalizeRotation();
  if(Bone!=Hip){Result.SetTranslation(Current.GetTranslation());Result.SetScale3D(Current.GetScale3D());}
  LastPose[I]=Result;
  if(Bone==Hip)NewHip=Result;
  else
  {
   const auto Parent=Bones.GetParentBoneIndex(Bone);
   const auto* CorrectedParent=Corrected.Find(Parent.GetInt());
   Result=Result*(CorrectedParent?*CorrectedParent:Output.Pose.GetComponentSpaceTransform(Parent));
  }
  Corrected.Add(Bone.GetInt(),Result);Out.Add(FBoneTransform(Bone,Result));
 }
 // Keep aim, shoulders and both hands as one rigid upper body. Translate them
 // with the new hip, without importing the source pistol arm pose or hip yaw.
 FTransform Upper=Output.Pose.GetComponentSpaceTransform(Chest);
 Upper.AddToTranslation(NewHip.GetLocation()-OriginalHip.GetLocation());
 Out.Add(FBoneTransform(Chest,Upper));Out.Sort(FCompareBoneTransformIndex());
}
