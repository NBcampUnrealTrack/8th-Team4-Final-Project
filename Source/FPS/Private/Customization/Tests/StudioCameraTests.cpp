#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Customization/CustomizationStudioActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioCameraTest,"FPS.Customization.Camera.FramingAndPan",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStudioCameraTest::RunTest(const FString&)
{
 UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::Editor){World=C.World();break;}
 if(!TestNotNull(TEXT("world"),World))return false;
 auto* A=World->SpawnActor<ACustomizationStudioActor>();A->bUseMutableLevelFraming=true;
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/MetaHumans/Dean/Face/SKM_Dean_FaceMesh"));
 if(!TestNotNull(TEXT("Dean face"),Mesh)){A->Destroy();return false;}
 A->Face->SetDisablePostProcessBlueprint(true);A->Face->SetSkeletalMesh(Mesh);A->Face->RefreshBoneTransforms();
 TestTrue(TEXT("left eye landmark exists"),A->Face->DoesSocketExist(TEXT("FACIAL_L_Eye")));
 A->SetFaceView(true);
 const FVector Eyes=(A->Face->GetSocketLocation(TEXT("FACIAL_L_Eye"))+A->Face->GetSocketLocation(TEXT("FACIAL_R_Eye")))*.5;
 TestTrue(TEXT("face camera centers near eyes rather than fixed chest height"),FMath::Abs(A->Camera->GetComponentLocation().Z-Eyes.Z)<5.);
 const float Before=A->Camera->GetComponentLocation().Z;
 A->Face->AddLocalOffset(FVector(0,0,30));A->SetFaceView(true);
 TestTrue(TEXT("framing follows translated face"),FMath::IsNearlyEqual(A->Camera->GetComponentLocation().Z-Before,30.,.1));
 const FVector Center=A->Camera->GetRelativeLocation();A->ZoomPreview(-80);
 TestTrue(TEXT("zoom preserves face height"),FMath::IsNearlyEqual(A->Camera->GetRelativeLocation().Z,Center.Z,.1));
 {
  const FVector Start=A->Camera->GetRelativeLocation();A->PanPreview(FVector2D(.1,.1));
  TestTrue(TEXT("right drag moves framing horizontally and vertically"),A->Camera->GetRelativeLocation().X>Start.X&&A->Camera->GetRelativeLocation().Z>Start.Z);
  A->SetFaceView(true);TestTrue(TEXT("face button resets pan and zoom"),A->Camera->GetRelativeLocation().Equals(Center,.1));
 }
 A->Destroy();return true;
}
#endif
