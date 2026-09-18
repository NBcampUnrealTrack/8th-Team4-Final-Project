#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Customization/CustomizationStudioActor.h"
#include "MuCO/CustomizableObject.h"
#include "Components/SkeletalMeshComponent.h"
#include "GroomComponent.h"
#include "Editor.h"

class FStudioPreviewWait : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 TWeakObjectPtr<ACustomizationStudioActor> Actor;
 int32 Stage=0;
 double Deadline=0;
public:
 FStudioPreviewWait(FAutomationTestBase* InTest,ACustomizationStudioActor* InActor):Test(InTest),Actor(InActor){Deadline=FPlatformTime::Seconds()+90;}
 virtual bool Update() override
 {
  if(!Actor.IsValid()){Test->AddError(TEXT("Preview actor lost"));return true;}
  auto* A=Actor.Get();A->Tick(.2f);
  if(A->IsUpdating()){if(FPlatformTime::Seconds()<Deadline)return false;Test->AddError(TEXT("Preview timeout"));A->Destroy();return true;}
  Test->TestEqual(TEXT("latest revision applied"),A->GetAppliedRevision(),Stage<3?Stage+1:6);
  Test->TestNotNull(TEXT("body generated"),A->Body->GetSkeletalMeshAsset());Test->TestNotNull(TEXT("face generated"),A->Face->GetSkeletalMeshAsset());Test->TestNotNull(TEXT("outfit generated"),A->Outfit->GetSkeletalMeshAsset());
  TArray<UGroomComponent*> Grooms;A->GetComponents(Grooms);bool Visible=false;for(auto* G:Grooms)if(G->GetFName()==TEXT("Hair"))Visible=G->IsVisible()&&G->GroomAsset!=nullptr;Test->TestTrue(TEXT("compatible default hair attached"),Visible);
  ++Stage;FCustomizationAppearance D;
  if(Stage<3){D.BodyType=Stage==1?TEXT("Custom"):TEXT("Fab");A->RequestAppearance(D);}
  else if(Stage==3){A->RequestAppearance(D);A->Tick(.2f);D.BodyType=TEXT("Fab");A->RequestAppearance(D);D.BodyType=TEXT("Custom");A->RequestAppearance(D);}
  else{A->Destroy();return true;}
  Deadline=FPlatformTime::Seconds()+90;return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomizationPreviewTest,"FPS.Customization.Preview",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCustomizationPreviewTest::RunTest(const FString&)
{
 auto* Config=LoadObject<UCustomizationStudioConfig>(nullptr,TEXT("/Game/FPS/Customizable/Character/Preview/DA_CustomizationStudio"));
 if(!TestNotNull(TEXT("config exists"),Config)||!Config->CustomizableObject)return false;
 FCompileParams P;P.bAsync=false;P.bSkipIfNotOutOfDate=false;Config->CustomizableObject->Compile(P);
 auto* World=GEditor->GetEditorWorldContext().World();if(!World)return false;
 auto* Actor=World->SpawnActor<ACustomizationStudioActor>();Actor->Config=Config;
 for(FString Type:{TEXT("Seo"),TEXT("Custom"),TEXT("Fab")}){TestTrue(TEXT("default hair valid"),Actor->IsHairValid(Type,TEXT("Default")));TestFalse(TEXT("unknown hair rejected"),Actor->IsHairValid(Type,TEXT("Missing")));}
 Actor->RequestAppearance(FCustomizationAppearance());ADD_LATENT_AUTOMATION_COMMAND(FStudioPreviewWait(this,Actor));return true;
}
#endif
