#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Animation/AnimNode_GroundFeet.h"
#include "Animation/AnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

struct FGroundFeetWorldTestAccess
{
 static bool Grounded(const FAnimNode_GroundFeet& Node){return Node.bGrounded;}
 static bool Hit(const FAnimNode_GroundFeet& Node,int32 Index){return Node.bHit[Index];}
 static FVector Point(const FAnimNode_GroundFeet& Node,int32 Index){return Node.FloorPoint[Index];}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFootGroundingWorldTest,"FPS.Animation.FootGrounding.WorldTrace",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFootGroundingWorldTest::RunTest(const FString&)
{
 const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
 AActor* Floor=World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);
 Floor->SetRootComponent(Box);Box->SetBoxExtent(FVector(500,500,10));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Floor->SetActorLocation(FVector(0,0,-10));
 const TCHAR* Paths[]={TEXT("/Game/MetaHumans/MHC_Seo/Body/SKM_MHC_Seo_BodyMesh"),TEXT("/Game/MetaHumans/Mason/Body/SKM_Mason_BodyMesh"),TEXT("/Game/CharacterAssemblies/MHC_Shrank/Body/SKM_MHC_Shrank_BodyMesh")};
 for(const TCHAR* Path:Paths)
 {
  auto* Character=World->SpawnActor<ACharacter>(FVector(0,0,88),FRotator::ZeroRotator);
  Character->GetCapsuleComponent()->SetCapsuleHalfHeight(88);
  auto* Mesh=Character->GetMesh();Mesh->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,Path));Mesh->SetRelativeLocation(FVector(0,0,-88));Mesh->RefreshBoneTransforms();
  auto* Instance=NewObject<UAnimInstance>(Mesh);
  Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  FAnimNode_GroundFeet Node;Node.PreUpdate(Instance);
  TestTrue(TEXT("Actual walking character activates trace"),FGroundFeetWorldTestAccess::Grounded(Node));
  for(int32 I=0;I<2;++I)
  {
   TestTrue(TEXT("Foot trace hits physical floor"),FGroundFeetWorldTestAccess::Hit(Node,I));
   TestTrue(TEXT("Trace returns floor surface, not own capsule"),FMath::IsNearlyZero(FGroundFeetWorldTestAccess::Point(Node,I).Z,.01));
  }
  Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);Node.PreUpdate(Instance);
  TestFalse(TEXT("Airborne character deactivates grounding"),FGroundFeetWorldTestAccess::Grounded(Node));
  TestFalse(TEXT("Airborne does not retain floor hit"),FGroundFeetWorldTestAccess::Hit(Node,0));
  Character->Destroy();
 }
 World->DestroyWorld(false);
 return true;
}
#endif
