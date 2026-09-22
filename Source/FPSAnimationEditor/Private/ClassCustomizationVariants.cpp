#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Commandlets/Commandlet.h"
#include "Materials/MaterialInterface.h"
namespace ClassCO {
UEdGraph* Graph(UCustomizableObject*);
FString Property(UObject*,const TCHAR*);
void Set(UObject*,const TCHAR*,const FString&);
void Save(UObject*);
UEdGraphNode* Node(UEdGraph*,const TCHAR*,int32,int32);
void Link(UEdGraph*,UEdGraphNode*,FName,UEdGraphNode*,FName);
}
namespace ClassColors { void Apply(UCustomizableObject*); }
namespace ClassVariants {
static const TCHAR* Classes[]={TEXT("Trooper"),TEXT("Vanguard")};
static const TCHAR* Bodies[2][3]={{TEXT("MHC_Frey"),TEXT("MHC_Gavin"),TEXT("Mason")},{TEXT("MHC_Crowd80"),TEXT("Dean"),TEXT("MHC_Shrank")}};
static const TCHAR* Parts[]={TEXT("Body"),TEXT("Face"),TEXT("Outfit")};
static FString MeshPath(int32 C,int32 B,int32 P)
{
 const FString N=Bodies[C][B];return TEXT("/Game/MetaHumans/")+N+(P==2?TEXT("/Clothing/")+N+TEXT("_Outfits"):FString(TEXT("/"))+Parts[P]+TEXT("/SKM_")+N+TEXT("_")+Parts[P]+TEXT("Mesh"));
}
static TAutoConsoleVariable<int32> Failures(TEXT("FPS.ClassVariants.Failures"),-1,TEXT("Last verification failure count"));
static void Build()
{
 using namespace ClassCO;
 USkeletalMesh* Sources[2][3][3];
 for(int32 C=0;C<2;++C)for(int32 B=0;B<3;++B)for(int32 P=0;P<3;++P)
 {Sources[C][B][P]=LoadObject<USkeletalMesh>(nullptr,*MeshPath(C,B,P));check(Sources[C][B][P]&&Sources[C][B][P]->GetImportedModel());}
 for(int32 C=0;C<2;++C)
 {
  auto* CO=LoadObject<UCustomizableObject>(nullptr,*(FString(TEXT("/Game/FPS/Customizable/CO_"))+Classes[C]));check(CO);CO->AddToRoot();
  auto* G=Graph(CO);check(G);UEdGraphNode* Root=nullptr;TMap<FString,UEdGraphNode*> Parameters;
  const auto Old=G->Nodes;
  for(UEdGraphNode* N:Old)
  {
   for(auto* Pin:N->Pins)Pin->BreakAllPinLinks();
   const FString Parameter=Property(N,TEXT("ParameterName"));
   if(N->GetClass()->GetName()==TEXT("CustomizableObjectNodeObject"))Root=N;
   else if(Parameter==TEXT("BodyType")||Parameter==TEXT("SkinColor")||Parameter==TEXT("SkinValue")||Parameter==TEXT("EyeColor"))Parameters.Add(Parameter,N);
   else G->RemoveNode(N);
  }
  check(Root);auto* Enum=Parameters.FindChecked(TEXT("BodyType"));
  Set(Enum,TEXT("Values"),FString::Printf(TEXT("((Name=\"%s\"),(Name=\"%s\"),(Name=\"%s\"))"),Bodies[C][0],Bodies[C][1],Bodies[C][2]));
  Set(Enum,TEXT("DefaultIndex"),TEXT("0"));
  for(int32 P=0;P<3;++P)
  {
   int32 LODCount=0;UEdGraphNode* MeshNodes[3];
   for(int32 B=0;B<3;++B){LODCount=FMath::Max(LODCount,Sources[C][B][P]->GetLODNum());MeshNodes[B]=Node(G,TEXT("SkeletalMesh"),-1600+B*300,P*6000);Set(MeshNodes[B],TEXT("SkeletalMesh"),Sources[C][B][P]->GetPathName());}
   auto* Component=Node(G,TEXT("ComponentMesh"),1400,P*6000);
   Set(Component,TEXT("ComponentName"),Parts[P]);Set(Component,TEXT("ReferenceSkeletalMesh"),Sources[C][0][P]->GetPathName());Set(Component,TEXT("NumLODs"),FString::FromInt(LODCount));Set(Component,TEXT("AutoLODStrategy"),TEXT("Manual"));Link(G,Component,TEXT("Component"),Root,TEXT("Components"));
   int32 Row=0;
   for(int32 L=0;L<LODCount;++L)
   {
    int32 Sections=0;for(int32 B=0;B<3;++B){const auto& Models=Sources[C][B][P]->GetImportedModel()->LODModels;Sections=FMath::Max(Sections,Models[FMath::Min(L,Models.Num()-1)].Sections.Num());}
    for(int32 S=0;S<Sections;++S)
    {
     auto* Switch=Node(G,TEXT("MaterialSwitch"),900,P*6000+Row*200);Link(G,Enum,TEXT("Enum"),Switch,TEXT("Switch Parameter"));
     for(int32 B=0;B<3;++B)
     {
      auto* Mesh=Sources[C][B][P];const auto& Models=Mesh->GetImportedModel()->LODModels;const int32 SourceLOD=FMath::Min(L,Models.Num()-1);const auto& SS=Models[SourceLOD].Sections;
      if(!SS.IsValidIndex(S))continue;
      UMaterialInterface* Material=Mesh->GetMaterials()[SS[S].MaterialIndex].MaterialInterface;check(Material);
      auto* Mat=Node(G,TEXT("Material"),-500+B*400,P*6000+Row*200);Set(Mat,TEXT("Material"),Material->GetPathName());
      Link(G,MeshNodes[B],*FString::Printf(TEXT("LOD %d - Section %d - Mesh"),SourceLOD,S),Mat,TEXT("Mesh_Input_Pin"));
      auto Connect=[&](const TCHAR* Pin,const TCHAR* Param,const TCHAR* Output){if(Mat->FindPin(Pin))Link(G,Parameters.FindChecked(Param),Output,Mat,Pin);};
      Connect(TEXT("Basecolor Global Multiply Post-Bake"),TEXT("SkinColor"),TEXT("Color"));Connect(TEXT("Basecolor Global Value Post-Bake"),TEXT("SkinValue"),TEXT("Float"));Connect(TEXT("Iris Color Multiply"),TEXT("EyeColor"),TEXT("Color"));
      Link(G,Mat,TEXT("Mesh Section_Output_Pin"),Switch,*FString::Printf(TEXT("Material %d "),B));
     }
     Link(G,Switch,TEXT("Material"),Component,*FString::Printf(TEXT("LOD %d"),L));++Row;
    }
   }
  }
  ClassColors::Apply(CO);
  G->NotifyGraphChanged();CO->PostEditChange();FCompileParams Params;Params.bAsync=false;Params.bSkipIfNotOutOfDate=false;CO->Compile(Params);Save(CO);CO->RemoveFromRoot();
 }
}
static FAutoConsoleCommand BuildCommand(TEXT("FPS.ClassVariants.Build"),TEXT("Rebuild approved Trooper/Vanguard variant graphs"),FConsoleCommandDelegate::CreateStatic(&Build));
static void Verify()
{
 int32 Errors=0;
 for(int32 C=0;C<2;++C)
 {
  auto* CO=LoadObject<UCustomizableObject>(nullptr,*(FString(TEXT("/Game/FPS/Customizable/CO_"))+Classes[C]));check(CO);CO->AddToRoot();
  bool Options=false;
  for(UEdGraphNode* N:ClassCO::Graph(CO)->Nodes)if(ClassCO::Property(N,TEXT("ParameterName"))==TEXT("BodyType"))
  {const FString V=ClassCO::Property(N,TEXT("Values"));Options=true;for(int32 B=0;B<3;++B)Options&=V.Contains(Bodies[C][B]);}
  if(!Options){++Errors;UE_LOG(LogTemp,Error,TEXT("CLASS_VARIANTS_MISSING_OPTIONS %s"),Classes[C]);CO->RemoveFromRoot();continue;}
  FCompileParams P;P.bAsync=false;P.bSkipIfNotOutOfDate=false;CO->Compile(P);
  auto* I=CO->CreateInstance();check(I);I->AddToRoot();
  for(int32 B=0;B<3;++B)
  {
   I->SetIntParameterSelectedOption(TEXT("BodyType"),Bodies[C][B]);
   auto Done=MakeShared<bool>(false);auto OK=MakeShared<bool>(false);
   FInstanceUpdateNativeDelegate D;D.AddLambda([Done,OK](const FUpdateContext& Context){*Done=true;*OK=Context.UpdateResult==EUpdateResult::Success||Context.UpdateResult==EUpdateResult::Warning;});
   I->UpdateSkeletalMeshAsyncResult(D,true,true);double End=FPlatformTime::Seconds()+180.;
   while(!*Done&&FPlatformTime::Seconds()<End&&!IsEngineExitRequested())CommandletHelpers::TickEngine();
   bool Valid=*Done&&*OK;
   for(int32 Part=0;Part<3;++Part)
   {
    auto* M=I->GetSkeletalMeshComponentSkeletalMesh(Parts[Part]);auto* R=M?M->GetResourceForRendering():nullptr;
    Valid&=R&&R->LODRenderData.Num()>0;
    if(R)for(const auto& L:R->LODRenderData)Valid&=L.GetNumVertices()>0&&L.RenderSections.Num()>0;
    auto* Source=LoadObject<USkeletalMesh>(nullptr,*MeshPath(C,B,Part));
    if(R&&Source&&R->LODRenderData.Num()){const auto* S=Source->GetResourceForRendering();Valid&=S&&R->LODRenderData[0].GetTotalFaces()==S->LODRenderData[0].GetTotalFaces();}
   }
   if(!Valid)++Errors;
   UE_LOG(LogTemp,Display,TEXT("CLASS_VARIANT %s %s valid=%d"),Classes[C],Bodies[C][B],Valid);
  }
  I->RemoveFromRoot();CO->RemoveFromRoot();
 }
 Failures->Set(Errors);if(Errors){UE_LOG(LogTemp,Error,TEXT("CLASS_VARIANTS_FAILURES %d"),Errors);}else{UE_LOG(LogTemp,Display,TEXT("CLASS_VARIANTS_VERIFIED"));}
}
static FAutoConsoleCommand VerifyCommand(TEXT("FPS.ClassVariants.Verify"),TEXT("Verify six generated variants"),FConsoleCommandDelegate::CreateStatic(&Verify));
}
