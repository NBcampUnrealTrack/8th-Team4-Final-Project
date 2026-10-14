#include "Animation/AnimNode_AlignLowerBody.h"
#include "Animation/AnimInstance.h"
#include "Character/ShooterCharacter.h"
#include "Character/CharacterPresentationTags.h"
#include "Combat/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Player/DominationPlayerState.h"

FAnimNode_AlignLowerBody::FAnimNode_AlignLowerBody()
{
 Pelvis.BoneName=TEXT("pelvis");Spine.BoneName=TEXT("spine_01");
}
void FAnimNode_AlignLowerBody::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
 FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);bEligible=false;
}
void FAnimNode_AlignLowerBody::PreUpdate(const UAnimInstance* Instance)
{
 bEligible=false;CombatClass=EDominationCombatClass::Unselected;
 const auto* C=Cast<AShooterCharacter>(Instance->TryGetPawnOwner());
 const auto* Mesh=Instance->GetSkelMeshComponent();
 if(!bEnabled||!C||Mesh!=C->GetMesh()||Mesh->IsSimulatingPhysics()||!C->GetCombatComponent()||!C->GetCombatComponent()->GetCurrentWeapon())return;
 if(const auto* State=C->GetPlayerState<ADominationPlayerState>())CombatClass=State->GetCombatClass();
 if(CombatClass==EDominationCombatClass::Unselected)
  for(const auto& Def:GetDefault<UDominationClassSettings>()->CombatClasses)
   if(Def.PawnClass.Get()&&C->IsA(Def.PawnClass.Get())){CombatClass=Def.CombatClass;break;}
 const auto* Move=C->GetCharacterMovement();
 // 클래스 선택 Pawn은 낙하/이동 방지를 위해 MOVE_None을 쓴다.
 // 명시적으로 표시된 정지 미리보기에만 게임의 서 있는 자세 보정을 허용한다.
 const bool bStationaryPreview=Move&&Move->MovementMode==MOVE_None
  &&C->ActorHasTag(CharacterPresentationTags::ClassSelectionPreview);
 bEligible=PrimaryStances.Contains(CombatClass)&&Move&&(Move->IsMovingOnGround()||bStationaryPreview)&&!C->bIsCrouched
  &&C->GetVelocity().SizeSquared2D()<9.f&&Move->GetCurrentAcceleration().IsNearlyZero();
 // The fixed-body stance is coherent with its upper pose. A preview camera
 // may rotate the whole pawn, but must never counter-rotate only its legs.
}
void FAnimNode_AlignLowerBody::UpdateInternal(const FAnimationUpdateContext& Context)
{
 FAnimNode_SkeletalControlBase::UpdateInternal(Context);
}
void FAnimNode_AlignLowerBody::InitializeBoneReferences(const FBoneContainer& Bones)
{
 Pelvis.Initialize(Bones);Spine.Initialize(Bones);
}
bool FAnimNode_AlignLowerBody::IsValidToEvaluate(const USkeleton*,const FBoneContainer& Bones)
{
 return bEnabled&&Pelvis.IsValidToEvaluate(Bones)&&Spine.IsValidToEvaluate(Bones);
}
void FAnimNode_AlignLowerBody::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out)
{
 if(!bEligible)return;
 const auto* Stance=PrimaryStances.Find(CombatClass);if(!Stance)return;
 const auto* Hip=Stance->Bones.Find(TEXT("pelvis"));if(!Hip)return;
 const auto& Bones=Output.Pose.GetPose().GetBoneContainer();
 const auto P=Pelvis.GetCompactPoseIndex(Bones),S=Spine.GetCompactPoseIndex(Bones);
 FTransform Upper=Output.Pose.GetComponentSpaceTransform(S);
 FTransform PelvisPose=Output.Pose.GetComponentSpaceTransform(P);
 const FVector OriginalPelvisLocation=PelvisPose.GetLocation();
 PelvisPose.SetRotation(Hip->GetRotation());PelvisPose.SetTranslation(Hip->GetTranslation());
 // bIsCrouched changes before the input animation finishes standing up.
 // Keep the pelvis/spine connection coherent when imposing the fixed stance:
 // the downstream crouch transition can then move both back together. Holding
 // the spine at its old component-space position would lower the torso twice.
 // Preserve upper-body orientation/aim; only follow the pelvis translation.
 Upper.AddToTranslation(PelvisPose.GetLocation()-OriginalPelvisLocation);
 TMap<int32,FTransform> Corrected;Corrected.Add(P.GetInt(),PelvisPose);Out.Add(FBoneTransform(P,PelvisPose));
 // Fixed primary rotations: equip/reload never changes the stance.
 // Preserve local translations/scales so limb lengths are not stretched.
 for(FCompactPoseBoneIndex I:Output.Pose.GetPose().ForEachBoneIndex())
 {
  if(I==P)continue;
  const auto Name=Bones.GetReferenceSkeleton().GetBoneName(Bones.MakeMeshPoseIndex(I).GetInt());
  const auto* Target=Stance->Bones.Find(Name);if(!Target)continue;
  const auto Parent=Bones.GetParentBoneIndex(I);
  const auto* ParentPose=Corrected.Find(Parent.GetInt());if(!ParentPose)continue;
  FTransform Local=Output.Pose.GetLocalSpaceTransform(I);Local.SetRotation(Target->GetRotation());
  Local.SetTranslation(Target->GetTranslation().GetSafeNormal()*Local.GetTranslation().Size());
  // Hip spacing and toe placement belong to the standing pose; femur/shin
  // segment lengths remain those of the current body.
  if(Name==TEXT("thigh_l")||Name==TEXT("thigh_r")||Name==TEXT("ball_l")||Name==TEXT("ball_r"))Local.SetTranslation(Target->GetTranslation());
  FTransform Result=Local*(*ParentPose);Result.NormalizeRotation();
  Corrected.Add(I.GetInt(),Result);Out.Add(FBoneTransform(I,Result));
 }
 Out.Add(FBoneTransform(S,Upper));Out.Sort(FCompareBoneTransformIndex());
}
