#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_GroundFeet.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
UEdGraphPin* PosePin(UEdGraphNode* Node,EEdGraphPinDirection Direction)
{
 for(auto* Pin:Node->Pins)if(Pin->Direction==Direction&&Pin->PinType.PinCategory==TEXT("struct")&&Pin->PinType.PinSubCategoryObject.IsValid()
  &&Pin->PinType.PinSubCategoryObject->GetName().Contains(TEXT("PoseLink")))return Pin;
 return nullptr;
}
template<typename T>T* MakeNode(UEdGraph* Graph,int32 X,int32 Y)
{
 auto* Node=NewObject<T>(Graph);Graph->AddNode(Node,false,false);Node->CreateNewGuid();Node->PostPlacedNewNode();Node->AllocateDefaultPins();Node->NodePosX=X;Node->NodePosY=Y;return Node;
}
void InstallGrounding()
{
 const FString Asset=TEXT("/Game/MetaHumans/Anim/ThirdPersonLayered/Blueprints/ABP_MetaHuman_Layered");
 auto* BP=LoadObject<UAnimBlueprint>(nullptr,*Asset);if(!BP){UE_LOG(LogTemp,Error,TEXT("GROUND_INSTALL missing BP"));return;}
 TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UAnimGraphNode_Root* Root=nullptr;
 for(auto* Graph:Graphs)if(Graph->GetFName()==TEXT("AnimGraph"))for(UEdGraphNode* N:Graph->Nodes)
 {
  if(Cast<UAnimGraphNode_GroundFeet>(N)){UE_LOG(LogTemp,Display,TEXT("GROUND_INSTALL already installed"));return;}
  if(auto* R=Cast<UAnimGraphNode_Root>(N))Root=R;
 }
 if(!Root){UE_LOG(LogTemp,Error,TEXT("GROUND_INSTALL missing root"));return;}
 auto* In=PosePin(Root,EGPD_Input);if(!In||In->LinkedTo.Num()!=1){UE_LOG(LogTemp,Error,TEXT("GROUND_INSTALL ambiguous root"));return;}
 const FString Filename=FPackageName::LongPackageNameToFilename(Asset,FPackageName::GetAssetPackageExtension());
 const FString Backup=FPaths::ProjectSavedDir()/TEXT("Codex/BeforeGroundFeet/ABP_MetaHuman_Layered.uasset");
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
 if(!IFileManager::Get().FileExists(*Backup)&&IFileManager::Get().Copy(*Backup,*Filename)!=COPY_OK){UE_LOG(LogTemp,Error,TEXT("GROUND_INSTALL backup failed"));return;}
 auto* Source=In->LinkedTo[0];auto* Graph=Root->GetGraph();
 auto* Local=MakeNode<UAnimGraphNode_LocalToComponentSpace>(Graph,Root->NodePosX-600,Root->NodePosY);
 auto* IK=MakeNode<UAnimGraphNode_GroundFeet>(Graph,Root->NodePosX-400,Root->NodePosY);
 auto* Component=MakeNode<UAnimGraphNode_ComponentToLocalSpace>(Graph,Root->NodePosX-200,Root->NodePosY);
 IK->NodeComment=TEXT("Shared terrain IK — airborne bypass; existing poses remain upstream");
 auto* LocalIn=PosePin(Local,EGPD_Input);auto* LocalOut=PosePin(Local,EGPD_Output);
 auto* IKIn=PosePin(IK,EGPD_Input);auto* IKOut=PosePin(IK,EGPD_Output);
 auto* ComponentIn=PosePin(Component,EGPD_Input);auto* ComponentOut=PosePin(Component,EGPD_Output);
 if(!LocalIn||!LocalOut||!IKIn||!IKOut||!ComponentIn||!ComponentOut){UE_LOG(LogTemp,Error,TEXT("GROUND_INSTALL pose pins missing, not saved"));return;}
 In->BreakAllPinLinks();Source->MakeLinkTo(LocalIn);LocalOut->MakeLinkTo(IKIn);IKOut->MakeLinkTo(ComponentIn);ComponentOut->MakeLinkTo(In);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 if(BP->Status==BS_Error){UE_LOG(LogTemp,Error,TEXT("GROUND_INSTALL compile failed, not saved"));return;}
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 const bool Saved=UPackage::SavePackage(BP->GetOutermost(),BP,*Filename,Args);
 UE_LOG(LogTemp,Display,TEXT("GROUND_INSTALL saved=%d backup=%s"),Saved,*Backup);
}
}
class FFPSAnimationEditorModule:public IModuleInterface
{
 IConsoleObject* Command=nullptr;
public:
 virtual void StartupModule()override {Command=IConsoleManager::Get().RegisterConsoleCommand(TEXT("FPS.GroundFeet.Install"),TEXT("Install shared foot grounding into the layered third-person graph, backing up the asset first."),FConsoleCommandDelegate::CreateStatic(&InstallGrounding));}
 virtual void ShutdownModule()override {if(Command)IConsoleManager::Get().UnregisterConsoleObject(Command);}
};
IMPLEMENT_MODULE(FFPSAnimationEditorModule,FPSAnimationEditor)

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFootGroundingGraphTest,"FPS.Animation.FootGrounding.Graph",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFootGroundingGraphTest::RunTest(const FString&)
{
 auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/MetaHumans/Anim/ThirdPersonLayered/Blueprints/ABP_MetaHuman_Layered"));
 if(!TestNotNull(TEXT("Saved layered blueprint reloads"),BP))return false;
 TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UAnimGraphNode_Root* Root=nullptr;int32 Count=0;
 for(auto* G:Graphs)if(G->GetFName()==TEXT("AnimGraph"))for(UEdGraphNode* N:G->Nodes)
 {
  if(Cast<UAnimGraphNode_GroundFeet>(N))++Count;
  if(auto* R=Cast<UAnimGraphNode_Root>(N))Root=R;
 }
 TestEqual(TEXT("Exactly one shared foot control"),Count,1);
 auto Upstream=[](UEdGraphNode* N)->UEdGraphNode*{auto* Pin=N?PosePin(N,EGPD_Input):nullptr;return Pin&&Pin->LinkedTo.Num()==1?Pin->LinkedTo[0]->GetOwningNode():nullptr;};
 auto* ToLocal=Cast<UAnimGraphNode_ComponentToLocalSpace>(Upstream(Root));
 auto* IK=Cast<UAnimGraphNode_GroundFeet>(Upstream(ToLocal));
 auto* ToComponent=Cast<UAnimGraphNode_LocalToComponentSpace>(Upstream(IK));
 TestNotNull(TEXT("Root receives component-to-local"),ToLocal);
 TestNotNull(TEXT("Grounding is last skeletal control"),IK);
 TestNotNull(TEXT("Grounding receives local-to-component"),ToComponent);
 TestNotNull(TEXT("Existing authored graph remains connected upstream"),Upstream(ToComponent));
 FKismetEditorUtilities::CompileBlueprint(BP);
 TestTrue(TEXT("Saved graph compiles"),BP->Status!=BS_Error);
 return true;
}
#endif
