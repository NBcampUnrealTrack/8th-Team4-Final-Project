#include "Animation/AnimNode_GroundFeet.h"
#include "Animation/FootGrounding.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TwoBoneIK.h"

FAnimNode_GroundFeet::FAnimNode_GroundFeet()
{
 Pelvis.BoneName=TEXT("pelvis");LeftFoot.BoneName=TEXT("foot_l");RightFoot.BoneName=TEXT("foot_r");
 Knees[0].BoneName=TEXT("calf_l");Knees[1].BoneName=TEXT("calf_r");
 Thighs[0].BoneName=TEXT("thigh_l");Thighs[1].BoneName=TEXT("thigh_r");
}
void FAnimNode_GroundFeet::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
 FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
 GroundWeight=PelvisOffset=Offsets[0]=Offsets[1]=0;
 BendDirection[0]=BendDirection[1]=FVector::RightVector;
}
void FAnimNode_GroundFeet::PreUpdate(const UAnimInstance* Instance)
{
 bGrounded=false;bHit[0]=bHit[1]=false;
 const auto* Character=Cast<ACharacter>(Instance->TryGetPawnOwner());
 auto* Mesh=Instance->GetSkelMeshComponent();
 if(!Character||!Mesh||Mesh!=Character->GetMesh())return;
 auto* Move=Character->GetCharacterMovement();
 if(!Move||!Move->IsMovingOnGround()||Mesh->IsSimulatingPhysics())return;
 bGrounded=true;MeshWorld=Mesh->GetComponentTransform();
 const float BaseZ=Character->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(FPSFootGrounding),false,Character);
 TArray<AActor*> Attached;Character->GetAttachedActors(Attached,true,true);Query.AddIgnoredActors(Attached);
 const FName Feet[]={LeftFoot.BoneName,RightFoot.BoneName};
 for(int32 I=0;I<2;++I)
 {
  if(!Mesh->DoesSocketExist(Feet[I]))continue;
  FVector P=Mesh->GetSocketLocation(Feet[I]);P.Z=BaseZ;
  FHitResult Hit;
  bHit[I]=Mesh->GetWorld()->LineTraceSingleByChannel(Hit,P+FVector(0,0,45),P-FVector(0,0,45),ECC_Visibility,Query)&&Move->IsWalkable(Hit);
  if(bHit[I]){FloorPoint[I]=Hit.ImpactPoint;FloorNormal[I]=Hit.ImpactNormal;}
 }
}
void FAnimNode_GroundFeet::UpdateInternal(const FAnimationUpdateContext& Context)
{
 FAnimNode_SkeletalControlBase::UpdateInternal(Context);Delta=Context.GetDeltaTime();
 GroundWeight=FPSFootGrounding::GroundedWeight(GroundWeight,bGrounded,Delta);
 if(!bGrounded){PelvisOffset=Offsets[0]=Offsets[1]=0;}
}
void FAnimNode_GroundFeet::InitializeBoneReferences(const FBoneContainer& Bones)
{
 Pelvis.Initialize(Bones);LeftFoot.Initialize(Bones);RightFoot.Initialize(Bones);
 for(int32 I=0;I<2;++I){Knees[I].Initialize(Bones);Thighs[I].Initialize(Bones);}
 const FBoneReference* Feet[]={&LeftFoot,&RightFoot};
 for(int32 I=0;I<2;++I)
 {
  ReferenceFootZ[I]=0;
  if(!Feet[I]->IsValidToEvaluate(Bones))continue;
  int32 Index=Feet[I]->BoneIndex;FTransform Ref=FTransform::Identity;
  const auto& Skeleton=Bones.GetReferenceSkeleton();
  while(Index!=INDEX_NONE){Ref=Ref*Skeleton.GetRefBonePose()[Index];Index=Skeleton.GetParentIndex(Index);}
  ReferenceFootZ[I]=Ref.GetLocation().Z;
 }
}
bool FAnimNode_GroundFeet::IsValidToEvaluate(const USkeleton*,const FBoneContainer& Bones)
{
 return Pelvis.IsValidToEvaluate(Bones)&&LeftFoot.IsValidToEvaluate(Bones)&&RightFoot.IsValidToEvaluate(Bones)
 &&Knees[0].IsValidToEvaluate(Bones)&&Knees[1].IsValidToEvaluate(Bones)&&Thighs[0].IsValidToEvaluate(Bones)&&Thighs[1].IsValidToEvaluate(Bones);
}
void FAnimNode_GroundFeet::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out)
{
 if(GroundWeight<=KINDA_SMALL_NUMBER)return;
 const auto& Bones=Output.Pose.GetPose().GetBoneContainer();
 const FBoneReference* Feet[]={&LeftFoot,&RightFoot};
 FTransform Foot[2],Knee[2],Thigh[2];float Contacts[2];
 for(int32 I=0;I<2;++I)
 {
  Foot[I]=Output.Pose.GetComponentSpaceTransform(Feet[I]->GetCompactPoseIndex(Bones));
  Knee[I]=Output.Pose.GetComponentSpaceTransform(Knees[I].GetCompactPoseIndex(Bones));
  Thigh[I]=Output.Pose.GetComponentSpaceTransform(Thighs[I].GetCompactPoseIndex(Bones));
  Contacts[I]=FPSFootGrounding::ContactWeight(Foot[I].GetLocation().Z-ReferenceFootZ[I]);
  // Terrain displacement relative to the mesh's authored sole plane. Swing height is preserved.
  const float Target=bHit[I]?FPSFootGrounding::LimitedOffset(MeshWorld.InverseTransformPosition(FloorPoint[I]).Z-ReferenceSoleZ,MaxFootOffset)*Contacts[I]:0.f;
  Offsets[I]=FMath::FInterpTo(Offsets[I],Target,Delta,18.f);
 }
 const float TargetPelvis=FMath::Clamp(FMath::Min(Offsets[0],Offsets[1]),-MaxPelvisOffset,0.f);
 PelvisOffset=FMath::FInterpTo(PelvisOffset,TargetPelvis,Delta,15.f);
 const FVector Shift(0,0,PelvisOffset*GroundWeight);
 FTransform PelvisTransform=Output.Pose.GetComponentSpaceTransform(Pelvis.GetCompactPoseIndex(Bones));
 PelvisTransform.AddToTranslation(Shift);Out.Add(FBoneTransform(Pelvis.GetCompactPoseIndex(Bones),PelvisTransform));
 for(int32 I=0;I<2;++I)
 {
  const FVector Goal=Foot[I].GetLocation()+FVector(0,0,Offsets[I]*GroundWeight);
  // Follow the INPUT knee plane rather than an arbitrary sideways pole vector.
  const FVector Pole=FPSFootGrounding::StablePole(Thigh[I].GetLocation(),Knee[I].GetLocation(),Foot[I].GetLocation(),BendDirection[I])+Shift;
  Thigh[I].AddToTranslation(Shift);Knee[I].AddToTranslation(Shift);Foot[I].AddToTranslation(Shift);
  // Do not re-solve an unchanged leg: a nearly straight authored pose must not
  // pick up the fallback bend plane merely because the node is active.
  if(!Goal.Equals(Foot[I].GetLocation(),.0001))
   AnimationCore::SolveTwoBoneIK(Thigh[I],Knee[I],Foot[I],Pole,Goal,false,1.0,1.0);
  if(bHit[I])
  {
   FVector Normal=MeshWorld.InverseTransformVectorNoScale(FloorNormal[I]).GetSafeNormal();
   const FQuat Tilt=FQuat::FindBetweenNormals(FVector::UpVector,Normal);
   Foot[I].SetRotation((FQuat::Slerp(FQuat::Identity,Tilt,Contacts[I]*GroundWeight)*Foot[I].GetRotation()).GetNormalized());
  }
  Out.Add(FBoneTransform(Thighs[I].GetCompactPoseIndex(Bones),Thigh[I]));
  Out.Add(FBoneTransform(Knees[I].GetCompactPoseIndex(Bones),Knee[I]));
  Out.Add(FBoneTransform(Feet[I]->GetCompactPoseIndex(Bones),Foot[I]));
 }
 Out.Sort(FCompareBoneTransformIndex());
}
