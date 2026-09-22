#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Animation/FootGrounding.h"
#include "Animation/AnimNode_GroundFeet.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"

struct FGroundFeetTestAccess
{
 static void Set(FAnimNode_GroundFeet& Node,float Weight,float Left,float Right)
 {
  Node.GroundWeight=Weight;Node.Delta=1.f/60.f;Node.MeshWorld=FTransform::Identity;
  Node.bHit[0]=Node.bHit[1]=true;Node.FloorPoint[0]=FVector(0,0,Left);Node.FloorPoint[1]=FVector(0,0,Right);
  Node.FloorNormal[0]=Node.FloorNormal[1]=FVector::UpVector;
 }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFootGroundingSkeletonTest,"FPS.Animation.FootGrounding.Skeletons",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFootGroundingSkeletonTest::RunTest(const FString&)
{
 const TCHAR* MeshPaths[]={TEXT("/Game/MetaHumans/MHC_Seo/Body/SKM_MHC_Seo_BodyMesh"),TEXT("/Game/MetaHumans/Mason/Body/SKM_Mason_BodyMesh"),TEXT("/Game/CharacterAssemblies/MHC_Shrank/Body/SKM_MHC_Shrank_BodyMesh")};
 for(const TCHAR* Path:MeshPaths)
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,Path);if(!TestNotNull(TEXT("Actual class mesh"),Mesh))continue;
  FMemMark Mark(FMemStack::Get());TArray<FBoneIndexType> Indices;
  for(int32 I=0;I<Mesh->GetRefSkeleton().GetNum();++I)Indices.Add(I);
  FBoneContainer Bones(Indices,UE::Anim::FCurveFilterSettings(),*Mesh);
  FComponentSpacePoseContext Pose(nullptr);Pose.Pose.InitPose(&Bones);
  FAnimNode_GroundFeet Node;Node.InitializeBoneReferences(Bones);
  TestTrue(TEXT("Shared chain resolves for this class"),Node.IsValidToEvaluate(Mesh->GetSkeleton(),Bones));
  TArray<FBoneTransform> Result;
  FGroundFeetTestAccess::Set(Node,0,12,-10);Node.EvaluateSkeletalControl_AnyThread(Pose,Result);
  TestEqual(TEXT("Airborne produces no modifications at all"),Result.Num(),0);
  FGroundFeetTestAccess::Set(Node,1,0,0);Node.EvaluateSkeletalControl_AnyThread(Pose,Result);
  for(const auto& Bone:Result)TestTrue(TEXT("Flat terrain preserves reference pose"),Bone.Transform.Equals(Pose.Pose.GetComponentSpaceTransform(Bone.BoneIndex),.002));
  for(int32 Frame=0;Frame<90;++Frame)
  {
   Result.Reset();FGroundFeetTestAccess::Set(Node,1,8,-8);Node.EvaluateSkeletalControl_AnyThread(Pose,Result);
   for(const auto& Bone:Result)TestFalse(TEXT("Finite bone transform"),Bone.Transform.ContainsNaN());
  }
  auto Position=[&](FName Name,bool Modified)
  {
   FBoneReference Ref;Ref.BoneName=Name;Ref.Initialize(Bones);const auto Index=Ref.GetCompactPoseIndex(Bones);
   if(Modified)for(const auto& Bone:Result)if(Bone.BoneIndex==Index)return Bone.Transform.GetLocation();
   return Pose.Pose.GetComponentSpaceTransform(Index).GetLocation();
  };
  for(const TCHAR* Side:{TEXT("l"),TEXT("r")})
  {
   const FName Thigh(*FString::Printf(TEXT("thigh_%s"),Side)),Knee(*FString::Printf(TEXT("calf_%s"),Side)),Foot(*FString::Printf(TEXT("foot_%s"),Side));
   TestTrue(TEXT("Thigh never stretches"),FMath::IsNearlyEqual(FVector::Distance(Position(Thigh,true),Position(Knee,true)),FVector::Distance(Position(Thigh,false),Position(Knee,false)),.002));
   TestTrue(TEXT("Calf never stretches"),FMath::IsNearlyEqual(FVector::Distance(Position(Knee,true),Position(Foot,true)),FVector::Distance(Position(Knee,false),Position(Foot,false)),.002));
  }
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFootGroundingAuthoredAnimationTest,"FPS.Animation.FootGrounding.AuthoredAnimations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFootGroundingAuthoredAnimationTest::RunTest(const FString&)
{
 // Catch a solver that rebuilds the source knee plane even when terrain requires
 // no correction, and any output that would replace the authored airborne pose.
 const TCHAR* Classes[]={TEXT("Ranger"),TEXT("Trooper"),TEXT("Vanguard")};
 const TCHAR* MeshPaths[]={TEXT("/Game/MetaHumans/MHC_Seo/Body/SKM_MHC_Seo_BodyMesh"),TEXT("/Game/MetaHumans/Mason/Body/SKM_Mason_BodyMesh"),TEXT("/Game/CharacterAssemblies/MHC_Shrank/Body/SKM_MHC_Shrank_BodyMesh")};
 for(int32 Class=0;Class<3;++Class)
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,MeshPaths[Class]);
  if(!TestNotNull(FString(Classes[Class])+TEXT(" source mesh"),Mesh))continue;
  FMemMark Mark(FMemStack::Get());TArray<FBoneIndexType> Indices;
  for(int32 I=0;I<Mesh->GetRefSkeleton().GetNum();++I)Indices.Add(I);
  FBoneContainer Bones(Indices,UE::Anim::FCurveFilterSettings(),*Mesh);
  const FString Root=TEXT("/Game/MetaHumans/Anim/ThirdPersonLayered/Locomotion/");
  const TArray<FString> Clips={Root+Classes[Class]+TEXT("/TP_Walk_Fwd"),
   Root+TEXT("TP_StableJump_")+Classes[Class]+TEXT("_Start"),
   Root+TEXT("TP_StableJump_")+Classes[Class]+TEXT("_Loop"),
   Root+TEXT("TP_StableJump_")+Classes[Class]+TEXT("_Land")};
  for(int32 Clip=0;Clip<Clips.Num();++Clip)
  {
   auto* Sequence=LoadObject<UAnimSequence>(nullptr,*Clips[Clip]);
   if(!TestNotNull(Clips[Clip],Sequence))continue;
   FAnimNode_GroundFeet Node;Node.InitializeBoneReferences(Bones);
   for(int32 Frame=0;Frame<9;++Frame)
   {
    FCompactPose Local;Local.SetBoneContainer(&Bones);Local.ResetToRefPose();
    FBlendedCurve Curve;Curve.InitFrom(Bones);
    UE::Anim::FStackAttributeContainer Attributes;
    FAnimationPoseData Data(Local,Curve,Attributes);
    Sequence->GetAnimationPose(Data,FAnimExtractContext(static_cast<double>(Sequence->GetPlayLength())*Frame/8.0));
    FComponentSpacePoseContext Pose(nullptr);Pose.Pose.InitPose(Local);
    TArray<FBoneTransform> Result;
    FGroundFeetTestAccess::Set(Node,Clip==0?1.f:0.f,Clip==0?0.f:12.f,Clip==0?0.f:-10.f);
    Node.EvaluateSkeletalControl_AnyThread(Pose,Result);
    const FString Sample=FString::Printf(TEXT("%s frame %d"),*Sequence->GetName(),Frame);
    if(Clip!=0)TestEqual(Sample+TEXT(" airborne has no bone replacements"),Result.Num(),0);
    else for(const auto& Bone:Result)
    {
     const FName Name=Bones.GetReferenceSkeleton().GetBoneName(Bones.MakeMeshPoseIndex(Bone.BoneIndex).GetInt());
     TestTrue(Sample+TEXT(" flat floor preserves ")+Name.ToString(),
      Bone.Transform.Equals(Pose.Pose.GetComponentSpaceTransform(Bone.BoneIndex),.01));
    }
   }
  }
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFootGroundingPolicyTest,"FPS.Animation.FootGrounding.Policy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFootGroundingPolicyTest::RunTest(const FString&)
{
 using namespace FPSFootGrounding;
 TestEqual(TEXT("Airborne must immediately leave authored jump untouched"),GroundedWeight(1.f,false,1.f/60.f),0.f);
 const float First=GroundedWeight(0.f,true,1.f/60.f);
 TestTrue(TEXT("Landing starts smoothly, not an instant snap"),First>0.f&&First<.3f);
 float Weight=0;for(int32 I=0;I<60;++I)Weight=GroundedWeight(Weight,true,1.f/60.f);
 TestTrue(TEXT("Grounded converges"),Weight>.99f);
 TestEqual(TEXT("Planted foot contact"),ContactWeight(0.f),1.f);
 TestEqual(TEXT("Raised swing foot must remain free"),ContactWeight(15.f),0.f);
 TestTrue(TEXT("Contact fades smoothly"),ContactWeight(8.f)>0.f&&ContactWeight(8.f)<1.f);
 TestEqual(TEXT("Down step bounded"),LimitedOffset(-100.f,15.f),-15.f);
 TestEqual(TEXT("Up step bounded"),LimitedOffset(100.f,15.f),15.f);
 TestEqual(TEXT("Flat floor does not alter source"),LimitedOffset(0.f,15.f),0.f);
 FVector Bend(0,1,0);
 const FVector Hip(0,0,100),Foot(0,0,0);
 for(float Side:{0.f,.0001f,-.0001f})
 {
  const FVector Pole=StablePole(Hip,FVector(Side,0,50),Foot,Bend);
  TestTrue(TEXT("Straight knee keeps a forward, non-singular bend plane"),Pole.Y>10.f);
 }
 return true;
}
#endif
