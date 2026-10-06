#include "Animation/AnimNode_CrouchTransition.h"
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
 bInitializedState=false;bCaptureStart=false;ActiveClip=nullptr;LastPose.Reset();StartPose.Reset();CachedBoneValid.Reset();Elapsed=Duration=0;MovementWeight=1;
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
  bInitializedState=true;bPreviousCrouched=bCrouched;ActiveClip=nullptr;Duration=0;bCaptureStart=false;return;
 }
 if(bCrouched!=bPreviousCrouched)
 {
  const float ReversePhase=ActiveClip&&Duration>0?1.f-FMath::Clamp((StartTime+Elapsed)/ActiveClip->GetPlayLength(),0.f,1.f):0.f;
  ActiveClip=bCrouched?EnterClip:ExitClip;
  StartTime=ReversePhase*ActiveClip->GetPlayLength();
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
 TMap<FName,FTransform> OldLast,OldStart;
 for(int32 I=0;I<LowerNames.Num();++I)
 {
  if(LastPose.IsValidIndex(I))OldLast.Add(LowerNames[I],LastPose[I]);
  if(StartPose.IsValidIndex(I))OldStart.Add(LowerNames[I],StartPose[I]);
 }
 Pelvis.Initialize(Bones);Spine.Initialize(Bones);LowerBones.Reset();LowerNames.Reset();CachedBoneValid.Reset();LastPose.Reset();StartPose.Reset();
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
   const auto* Last=OldLast.Find(Name);const auto* Start=OldStart.Find(Name);
   CachedBoneValid.Add(Last&&Start);LastPose.Add(Last?*Last:FTransform::Identity);StartPose.Add(Start?*Start:FTransform::Identity);
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
 if(LastPose.Num()!=LowerBones.Num()||CachedBoneValid.Num()!=LowerBones.Num())
 {
  LastPose.SetNum(LowerBones.Num());StartPose.SetNum(LowerBones.Num());CachedBoneValid.Init(false,LowerBones.Num());
 }
 for(int32 I=0;I<LowerBones.Num();++I)if(!CachedBoneValid[I])
 {
  const auto Bone=LowerBones[I];LastPose[I]=StartPose[I]=Bone==Hip?Output.Pose.GetComponentSpaceTransform(Bone):Output.Pose.GetLocalSpaceTransform(Bone);CachedBoneValid[I]=true;
 }
 if(bCaptureStart){StartPose=LastPose;bCaptureStart=false;}
 if(!bEligible||!ActiveClip||Duration<=0||StartPose.Num()!=LowerBones.Num())
 {
  for(int32 I=0;I<LowerBones.Num();++I)LastPose[I]=LowerBones[I]==Hip?Output.Pose.GetComponentSpaceTransform(Hip):Output.Pose.GetLocalSpaceTransform(LowerBones[I]);
  return;
 }
 FPoseContext Sample(Output.AnimInstanceProxy);
 FAnimationPoseData SampleData(Sample);
 ActiveClip->GetAnimationPose(SampleData,FAnimExtractContext(double(FMath::Min(StartTime+Elapsed,ActiveClip->GetPlayLength())),false));
 FCSPose<FCompactPose> SampleCS;SampleCS.InitPose(Sample.Pose);
 const float In=FMath::SmoothStep(0.f,FMath::Min(.16f,Duration),Elapsed);
 const float Tail=1.f-FMath::SmoothStep(FMath::Max(0.f,Duration-.22f),Duration,Elapsed);
 const FTransform OriginalHip=Output.Pose.GetComponentSpaceTransform(Hip);
 FTransform NewHip=OriginalHip;
 TMap<int32,FTransform> Corrected;
 for(int32 I=0;I<LowerBones.Num();++I)
 {
  const auto Bone=LowerBones[I];const FTransform Current=Bone==Hip?OriginalHip:Output.Pose.GetLocalSpaceTransform(Bone);
  FTransform Animated;Animated.Blend(StartPose[I],Bone==Hip?SampleCS.GetComponentSpaceTransform(Bone):Sample.Pose[Bone],In);
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
