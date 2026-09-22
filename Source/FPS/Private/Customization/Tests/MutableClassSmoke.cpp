#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationStudioController.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "UObject/UnrealType.h"
#include "Components/SkeletalMeshComponent.h"
#include "GroomComponent.h"
#include "GroomBindingAsset.h"
#include "GroomAsset.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GroomBlueprintLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Camera/CameraComponent.h"

// Explicit standalone visual probe; no profile/asset writes.
static FAutoConsoleCommandWithWorld ClassScreenshots(TEXT("FPS.MutableLevel.ClassScreenshots"),TEXT("Capture each class then exit"),FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
{
 if(!W||!W->GetMapName().Contains(TEXT("MutableLevel")))return;
 struct FState {FTimerHandle Timer;int32 Step=0,Seconds=0;bool bDiagnosed=false;};auto State=MakeShared<FState>();TWeakObjectPtr<UWorld> Weak(W);
 W->GetTimerManager().SetTimer(State->Timer,[Weak,State]()
 {
  if(!Weak.IsValid())return;auto* World=Weak.Get();++State->Seconds;
  auto* C=Cast<ACustomizationStudioController>(World->GetFirstPlayerController());auto* A=C?C->Studio.Get():nullptr;
  if(State->Seconds>100){UE_LOG(LogTemp,Error,TEXT("CLASS_VISUAL_TIMEOUT"));World->GetTimerManager().ClearTimer(State->Timer);FPlatformMisc::RequestExit(false);return;}
  if(!A||!A->IsAppearanceReady()||State->Seconds<8)return;
  if(FParse::Param(FCommandLine::Get(),TEXT("ClassColors"))||FParse::Param(FCommandLine::Get(),TEXT("CameraFraming")))
  {
   const bool bCamera=FParse::Param(FCommandLine::Get(),TEXT("CameraFraming"));
   if(State->Step>=36){UE_LOG(LogTemp,Display,TEXT("CLASS_COLORS_CAPTURED"));World->GetTimerManager().ClearTimer(State->Timer);FPlatformMisc::RequestExit(false);return;}
   const int32 Case=State->Step/4;
   if(State->Step%4==0)
   {
    const TCHAR* Classes[]={TEXT("Ranger"),TEXT("Trooper"),TEXT("Vanguard")};
    C->SelectClass(Classes[Case/3]);C->SelectBody(C->BodyChoices()[Case%3]);
    if(!bCamera){C->Draft.EyeColor=FLinearColor(0.01f,1.f,0.01f);
    for(int32 P=0;P<C->Draft.Colors.Num();++P){C->Draft.Colors[P]=P%2?FLinearColor(.02f,.1f,1.f):FLinearColor(1.f,.02f,.02f);C->Draft.Originals[P]=0.f;C->Draft.Values[P]=1.f;}}
    C->Changed();A->SetFaceView(true);if(!bCamera)A->ZoomPreview(120.f);
   }
   else if(State->Step%4==2){if(bCamera)A->ZoomPreview(-65.f);else A->SetFaceView(false);}
   else FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("%s_%02d_%s.png"),bCamera?TEXT("Camera"):TEXT("Colors"),State->Step,*C->Draft.BodyType),true,false);
   ++State->Step;return;
  }
  if(FParse::Param(FCommandLine::Get(),TEXT("FaceDiagnosis")))
  {
   A->SetFaceView(true);A->ZoomPreview(120.f);
   if(State->Step==0||State->Step==8)
   {
    C->SelectClass(State->Step==0?TEXT("Trooper"):TEXT("Vanguard"));C->SelectBody(C->BodyChoices()[2]);
    C->Draft.HairId=FParse::Param(FCommandLine::Get(),TEXT("FaceWithHair"))?A->HairChoices(C->Draft.BodyType)[State->Step==0?1:0].Id:FName(TEXT("Bald"));
    C->Changed();++State->Step;return;
   }
   if(State->Step==2||State->Step==10)
   {A->Face->SetDisablePostProcessBlueprint(true);A->Face->InitAnim(true);++State->Step;return;}
   if(State->Step==4||State->Step==12)
   {
    const FString Name=State->Step==4?TEXT("Mason"):TEXT("MHC_Shrank");
    auto* Source=LoadObject<USkeletalMesh>(nullptr,*(TEXT("/Game/MetaHumans/")+Name+TEXT("/Face/SKM_")+Name+TEXT("_FaceMesh")));
    auto* Generated=A->Face->GetSkeletalMeshAsset();
    UE_LOG(LogTemp,Display,TEXT("FACE_DIAG source=%s sourcePost=%s generatedPost=%s sourceBones=%d generatedBones=%d"),*Name,*GetPathNameSafe(Source->GetPostProcessAnimBlueprint()),*GetPathNameSafe(Generated->GetPostProcessAnimBlueprint()),Source->GetRefSkeleton().GetNum(),Generated->GetRefSkeleton().GetNum());
    A->Face->EmptyOverrideMaterials();A->Face->SetSkeletalMesh(Source);++State->Step;return;
   }
   if(State->Step==6){A->Face->SetDisablePostProcessBlueprint(false);State->Step=8;return;}
   if(State->Step==14){UE_LOG(LogTemp,Display,TEXT("FACE_DIAG_COMPLETE"));World->GetTimerManager().ClearTimer(State->Timer);FPlatformMisc::RequestExit(false);return;}
   FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("FaceDiag_%02d.png"),State->Step),true,false);++State->Step;return;
  }
  if(FParse::Param(FCommandLine::Get(),TEXT("ClassRoundTrip")))
  {
   A->SetFaceView(true);
   if(State->Step%2==0)
   {
    for(auto* Mesh:{A->Body.Get(),A->Face.Get(),A->Outfit.Get()})
    {
     UE_LOG(LogTemp,Display,TEXT("TRANSITION_MESH step=%d component=%s mesh=%s overrides=%d"),State->Step,*Mesh->GetName(),*GetNameSafe(Mesh->GetSkeletalMeshAsset()),Mesh->OverrideMaterials.Num());
     for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)UE_LOG(LogTemp,Display,TEXT("TRANSITION_MATERIAL %s slot=%d actual=%s asset=%s"),*Mesh->GetName(),Slot,*GetPathNameSafe(Mesh->GetMaterial(Slot)),*GetPathNameSafe(Mesh->GetSkeletalMeshAsset()->GetMaterials()[Slot].MaterialInterface));
    }
    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("Transition_%02d_%s.png"),State->Step,*C->ActiveClass.ToString()),true,false);++State->Step;return;
   }
   switch(State->Step)
   {
    case 1:C->SelectClass(TEXT("Trooper"));break;
    case 3:C->SelectClass(TEXT("Vanguard"));break;
    case 5:C->SelectClass(TEXT("Ranger"));break;
    case 7:C->SelectBody(C->BodyChoices()[2]);break;
    default:UE_LOG(LogTemp,Display,TEXT("TRANSITION_CAPTURED"));World->GetTimerManager().ClearTimer(State->Timer);FPlatformMisc::RequestExit(false);return;
   }
   ++State->Step;return;
  }
  if(State->Step==2&&!State->bDiagnosed)
  {
   State->bDiagnosed=true;UE_LOG(LogTemp,Display,TEXT("CLASS_HAIR_SUPPORTED %d"),UGroomBlueprintLibrary::IsHairStrandsSupportedInWorld(World));
   for(const TCHAR* Name:{TEXT("r.HairStrands.Enable"),TEXT("r.HairStrands.Strands"),TEXT("r.HairStrands.UseCardsInsteadOfStrands")})if(auto* V=IConsoleManager::Get().FindConsoleVariable(Name))UE_LOG(LogTemp,Display,TEXT("CLASS_HAIR_CVAR %s=%d"),Name,V->GetInt());
   TArray<UGroomComponent*> Grooms;A->GetComponents(Grooms);for(auto* G:Grooms)if(G->GetFName()==TEXT("Hair")){UE_LOG(LogTemp,Display,TEXT("CLASS_HAIR_STATE groom=%s visible=%d lod=%d bounds=%s"),*GetNameSafe(G->GroomAsset),G->IsVisible(),G->GetForcedLOD(),*G->Bounds.ToString());if(FParse::Param(FCommandLine::Get(),TEXT("ClassHairLOD0")))G->SetForcedLOD(0);if(FParse::Param(FCommandLine::Get(),TEXT("ClassHairUnbound")))G->SetBindingAsset(nullptr);}
   for(auto* G:Grooms)if(G->GetFName()==TEXT("Hair"))
   {
    UE_LOG(LogTemp,Display,TEXT("HAIR_RENDER_COMPONENT groups=%d material=%s cards=%d"),G->GetGroupCount(),*GetNameSafe(G->GetMaterial(0)),G->bUseCards);
    for(uint32 Group=0;Group<G->GetGroupCount();++Group)if(auto* Instance=G->GetGroupInstance(Group))
    {
     UE_LOG(LogTemp,Display,TEXT("HAIR_RENDER_INSTANCE geometry=%d binding=%d valid=%d strands=%d roots=%d proxy=%d forcedCards=%d predicted=%f meshLOD=%d selectedLOD=%f"),int32(Instance->GeometryType),int32(Instance->BindingType),Instance->IsValid(),Instance->Strands.IsValid(),Instance->Strands.HasValidRootData(),Instance->Debug.Proxy!=nullptr,Instance->bForceCards,Instance->Debug.LODPredictedIndex,Instance->Debug.MeshLODIndex,Instance->HairGroupPublicData?Instance->HairGroupPublicData->GetLODIndex():-9.f);
    }
   }
   return;
  }
  if(State->Step%2==0)
  {
   FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(TEXT("Class_")+C->ActiveClass.ToString()+TEXT(".png")),true,false);++State->Step;return;
  }
  if(State->Step==1){C->SelectClass(TEXT("Trooper"));++State->Step;return;}
  if(State->Step==3){C->SelectClass(TEXT("Vanguard"));++State->Step;return;}
  UE_LOG(LogTemp,Display,TEXT("CLASS_VISUAL_CAPTURED"));World->GetTimerManager().ClearTimer(State->Timer);FPlatformMisc::RequestExit(false);
 },3.f,true);
}));

class FClassPreviewCases : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 TWeakObjectPtr<ACustomizationStudioActor> Actor;
 struct FCase {UCustomizationStudioConfig* Config;FString Body;FName Hair;};
 TArray<FCase> Cases;
 int32 Index=0;
 double Deadline=0;
 bool bStarted=false;
public:
 FClassPreviewCases(FAutomationTestBase* T,ACustomizationStudioActor* A):Test(T),Actor(A)
 {
  for(const FName C:{FName(TEXT("Ranger")),FName(TEXT("Trooper")),FName(TEXT("Vanguard"))})
  {
   auto* Config=A->ClassConfigs.FindChecked(C).Get();
   for(const FString& Body:Config->BodyTypes)
   {
    Cases.Add({Config,Body,TEXT("Bald")});
    const auto Hair=ACustomizationStudioActor::HairChoicesForConfig(Config,Body);
    T->TestEqual(C.ToString()+Body+TEXT(" three hair choices"),Hair.Num(),3);
    for(const auto& H:Hair)Cases.Add({Config,Body,H.Id});
   }
  }
 }
 bool Update() override
 {
  if(!Actor.IsValid()){Test->AddError(TEXT("Preview lost"));return true;}
  auto* A=Actor.Get();
  if(Index>=Cases.Num()){A->Destroy();return true;}
  const auto& C=Cases[Index];
  if(!bStarted)
  {
   // Reproduce the card-only override serialized by the original Ranger map.
   TArray<UGroomComponent*> InitialGrooms;A->GetComponents(InitialGrooms);
   for(auto* G:InitialGrooms)G->SetUseCards(true);
   A->SetFaceView(true);A->SetClassConfig(C.Config);FCustomizationAppearance D;D.BodyType=C.Body;D.HairId=C.Hair;
   // Start one request then cross class twice while it is still in flight.
   // The returning first class must not accept the earlier generation either.
   if(Index==0)
   {
    A->RequestAppearance(D);A->Tick(.2f);
    auto* Other=A->ClassConfigs.FindChecked(TEXT("Trooper")).Get();A->SetClassConfig(Other);
    FCustomizationAppearance Temp;Temp.BodyType=Other->DefaultBodyType;Temp.HairId=Other->DefaultHairId;A->RequestAppearance(Temp);
    A->SetClassConfig(C.Config);
   }
   D.HairColor=FLinearColor(.2f,.3f,.4f);D.bUseOriginalHairColor=false;
   D.EyeColor=C.Hair==TEXT("Bald")?FLinearColor::White:FLinearColor(.01f,1.f,.01f);
   for(int32 P=0;P<D.Colors.Num();++P){D.Colors[P]=FLinearColor(.8f,.1f,.2f);D.Originals[P]=C.Hair==TEXT("Bald")?1.f:0.f;}
   A->RequestAppearance(D);Deadline=FPlatformTime::Seconds()+120.;bStarted=true;
  }
  A->Tick(.2f);
  if(A->IsUpdating())
  {
   if(FPlatformTime::Seconds()<Deadline)return false;
   Test->AddError(TEXT("Preview timeout: ")+C.Body);A->Destroy();return true;
  }
  const FString Label=C.Config->ClassId+TEXT("/")+C.Body+TEXT("/")+C.Hair.ToString();
  Test->TestTrue(Label+TEXT(" ready"),A->IsAppearanceReady());Test->TestTrue(Label+TEXT(" current config"),A->Config==C.Config);
  Test->TestTrue(Label+TEXT(" static preview disables face post process"),A->Face->GetDisablePostProcessBlueprint());
  const FVector Eyes=(A->Face->GetSocketLocation(TEXT("FACIAL_L_Eye"))+A->Face->GetSocketLocation(TEXT("FACIAL_R_Eye")))*.5;
  Test->TestTrue(Label+TEXT(" face camera follows completed generation"),FMath::Abs(A->Camera->GetComponentLocation().Z-Eyes.Z)<5.);
  const auto* LiveProperty=FindFProperty<FObjectPropertyBase>(A->GetClass(),TEXT("LiveInstance"));
  const auto* Live=LiveProperty?Cast<UCustomizableObjectInstance>(LiveProperty->GetObjectPropertyValue_InContainer(A)):nullptr;
  Test->TestNotNull(Label+TEXT(" live instance"),Live);
  for(auto* Mesh:{A->Body.Get(),A->Face.Get(),A->Outfit.Get()})
  {
   auto* M=Mesh->GetSkeletalMeshAsset();auto* R=M?M->GetResourceForRendering():nullptr;
   Test->TestTrue(Label+TEXT(" geometry"),Mesh->IsVisible()&&R&&!R->LODRenderData.IsEmpty()&&R->LODRenderData[0].GetNumVertices()>0);
   // A cached mesh may still hold an earlier instance's materials. The current
   // Mutable instance is the source of truth for each displayed material slot.
   if(Live)
   {
    const auto Materials=Live->GetSkeletalMeshComponentOverrideMaterials(Mesh->GetFName());
    for(int32 Slot=0;Slot<M->GetMaterials().Num();++Slot)
    {
     const auto* Expected=Materials.IsValidIndex(Slot)?Materials[Slot]:M->GetMaterials()[Slot].MaterialInterface.Get();
     const UMaterialInterface* Actual=Mesh->GetMaterial(Slot);
     // MetaHuman post-process animation may wrap the current material in a MID.
     while(Actual&&Actual!=Expected)
     {const auto* Dynamic=Cast<UMaterialInstance>(Actual);Actual=Dynamic?Dynamic->Parent.Get():nullptr;}
     Test->TestTrue(Label+Mesh->GetName()+FString::Printf(TEXT(" current material slot %d"),Slot),Actual==Expected);
    }
   }
  }
  int32 EyeSlots=0,ClothSlots=0;
  for(int32 Slot=0;Slot<A->Face->GetNumMaterials();++Slot)
  {
   FLinearColor Value;auto* Mat=A->Face->GetMaterial(Slot);
   if(Mat&&Mat->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Iris Color Multiply")),Value))
   {++EyeSlots;Test->TestTrue(Label+TEXT(" eye color or reset"),Value.Equals(C.Hair==TEXT("Bald")?FLinearColor::White:FLinearColor(.01f,1.f,.01f)));}
  }
  for(int32 Slot=0;Slot<A->Outfit->GetNumMaterials();++Slot)
  {
   FLinearColor Value;auto* Mat=A->Outfit->GetMaterial(Slot);
   if(Mat&&Mat->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ClassPartTint")),Value))
   {++ClothSlots;Test->TestTrue(Label+TEXT(" cloth color"),Value.Equals(FLinearColor(.8f,.1f,.2f)));float Original=-1.f;Mat->GetScalarParameterValue(FMaterialParameterInfo(TEXT("ClassUseOriginal")),Original);Test->TestEqual(Label+TEXT(" cloth dye or original"),Original,C.Hair==TEXT("Bald")?1.f:0.f);}
  }
  Test->TestTrue(Label+TEXT(" both eyes connected"),EyeSlots>=2);
  if(C.Config->ClassId!=TEXT("Ranger"))Test->TestTrue(Label+TEXT(" clothes connected"),ClothSlots>=4);
  TArray<UGroomComponent*> Grooms;A->GetComponents(Grooms);
  for(auto* G:Grooms)
  {
   const auto* Expected=C.Config->Grooms.FindByPredicate([&](const FStudioGroomChoice& H){return H.BodyType==C.Body&&H.Component==G->GetFName()&&(H.Component!=TEXT("Hair")||H.Id==C.Hair);});
   Test->TestEqual(Label+G->GetName()+TEXT(" visibility"),G->IsVisible(),Expected!=nullptr);
   if(Expected)
   {
    Test->TestTrue(Label+TEXT(" original groom"),G->GroomAsset==Expected->Groom);
    if(Expected->Groom->GetHairGroupsCards().IsEmpty())
     Test->TestFalse(Label+G->GetName()+TEXT(" strands-only groom must not force cards"),G->bUseCards);
    auto* Surface=Cast<USkeletalMeshComponent>(G->GetAttachParent());
    Test->TestTrue(Label+TEXT(" binding target"),Surface&&Surface->GetSkeletalMeshAsset()==Expected->Binding->GetTargetSkeletalMesh());
    Test->TestTrue(Label+TEXT(" nonrendered binding surface"),Surface&&Surface!=A->Face&&!Surface->bRenderInMainPass);
   }
  }
  UE_LOG(LogTemp,Display,TEXT("CLASS_PREVIEW_CASE %s"),*Label);++Index;bStarted=false;return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMutableClassSmoke,"FPS.Customization.Classes.PreviewCombinations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMutableClassSmoke::RunTest(const FString&)
{
 UWorld* W=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::Editor){W=Context.World();break;}
 if(!TestNotNull(TEXT("editor world"),W))return false;
 auto* A=W->SpawnActor<ACustomizationStudioActor>();A->bUseReferencePoseGrooms=true;A->bUseMutableLevelFraming=true;
 for(const TCHAR* Class:{TEXT("Ranger"),TEXT("Trooper"),TEXT("Vanguard")})
 {
  const FString Path=FString(Class)==TEXT("Ranger")?TEXT("/Game/FPS/Customizable/MutableLevel/DA_MutableLevelStudio"):FString(TEXT("/Game/FPS/Customizable/Character/Preview/DA_Studio"))+Class;
  auto* Config=LoadObject<UCustomizationStudioConfig>(nullptr,*Path);if(!TestNotNull(Class,Config)){A->Destroy();return false;}
  TestEqual(TEXT("three bodies"),Config->BodyTypes.Num(),3);TestEqual(TEXT("clothing controls"),Config->EnabledClothingParts.Num(),FString(Class)==TEXT("Ranger")?8:4);
  // Exercise the real startup path, not a test-only precompile of every class.
  A->ClassConfigs.Add(Class,Config);
 }
 ADD_LATENT_AUTOMATION_COMMAND(FClassPreviewCases(this,A));return true;
}
#endif
