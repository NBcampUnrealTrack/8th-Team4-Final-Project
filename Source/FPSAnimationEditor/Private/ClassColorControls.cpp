#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "MuCO/CustomizableObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"
namespace ClassCO {
UEdGraph* Graph(UCustomizableObject*);FString Property(UObject*,const TCHAR*);
void Set(UObject*,const TCHAR*,const FString&);void Save(UObject*);
UEdGraphNode* Node(UEdGraph*,const TCHAR*,int32,int32);
void Link(UEdGraph*,UEdGraphNode*,FName,UEdGraphNode*,FName);
UMaterialInterface* ClothingMaterial(UMaterialInterface*,const FString&,const FString&);
}
namespace ClassColors {
// Imported material slots verified against section bounds. Shared source materials
// are deliberately split by editable part without touching the imported assets.
static FString Group(const FString& Class,int32 Slot)
{
 if(Class==TEXT("Trooper"))
 {if(Slot==8)return TEXT("Top");if(Slot==2)return TEXT("Vest");if(Slot>=4&&Slot<=7)return TEXT("Boots");if(Slot==0||Slot==1||Slot==3)return TEXT("Bottom");}
 else {if(Slot==4)return TEXT("Top");if(Slot==3)return TEXT("Bottom");if(Slot==0)return TEXT("Boots");if(Slot==1||Slot==2)return TEXT("Attachment");}
 checkNoEntry();return {};
}
void Apply(UCustomizableObject* CO)
{
 using namespace ClassCO;
 const FString Class=CO->GetName().RightChop(3);check(Class==TEXT("Trooper")||Class==TEXT("Vanguard"));
 auto* G=Graph(CO);auto* Template=Graph(LoadObject<UCustomizableObject>(nullptr,TEXT("/Game/FPS/Customizable/CO_Ranger")));check(G&&Template);
 TMap<FString,UEdGraphNode*> Parameters;
 for(UEdGraphNode* N:G->Nodes){const auto P=Property(N,TEXT("ParameterName"));if(!P.IsEmpty())Parameters.Add(P,N);}
 const TArray<FString> Groups={TEXT("Top"),TEXT("Bottom"),Class==TEXT("Trooper")?TEXT("Vest"):TEXT("Attachment"),TEXT("Boots")};
 for(const auto& Part:Groups)for(const TCHAR* Suffix:{TEXT("Color"),TEXT("Value"),TEXT("UseOriginal")})
 {
  const FString Name=Part+Suffix;if(Parameters.Contains(Name))continue;
  UEdGraphNode* Ref=nullptr;for(UEdGraphNode* N:Template->Nodes)if(Property(N,TEXT("ParameterName"))==Name){Ref=N;break;}check(Ref);
  auto* N=Node(G,FString(Suffix)==TEXT("Color")?TEXT("ColorParameter"):TEXT("FloatParameter"),-2200,Parameters.Num()*180);
  Set(N,TEXT("ParameterName"),Name);Set(N,TEXT("DefaultValue"),Property(Ref,TEXT("DefaultValue")));Parameters.Add(Name,N);
 }
 const auto Nodes=G->Nodes;
 int32 Eyes=0,Outfits=0;TSet<USkeletalMesh*> EyeBodies,OutfitBodies;
 for(UEdGraphNode* N:Nodes)
 {
  if(N->GetClass()->GetName()!=TEXT("CustomizableObjectNodeMaterial"))continue;
  auto* Input=N->FindPin(TEXT("Mesh_Input_Pin"));if(!Input||Input->LinkedTo.Num()!=1)continue;
  auto* Upstream=Input->LinkedTo[0];auto* MeshNode=Upstream->GetOwningNode();
  auto* MeshProperty=FindFProperty<FObjectPropertyBase>(MeshNode->GetClass(),TEXT("SkeletalMesh"));
  auto* Mesh=MeshProperty?Cast<USkeletalMesh>(MeshProperty->GetObjectPropertyValue_InContainer(MeshNode)):nullptr;if(!Mesh)continue;
  TArray<FString> Tokens;Upstream->PinName.ToString().ParseIntoArray(Tokens,TEXT(" - "));check(Tokens.Num()>=3);
  const int32 LOD=FCString::Atoi(*Tokens[0].RightChop(4)),Section=FCString::Atoi(*Tokens[1].RightChop(8));
  const int32 Slot=Mesh->GetImportedModel()->LODModels[LOD].Sections[Section].MaterialIndex;
  auto* Original=Mesh->GetMaterials()[Slot].MaterialInterface.Get();check(Original);
  UMaterialInterface* Replacement=nullptr;FString Part;
  if(Mesh->GetPathName().Contains(TEXT("/Clothing/")))
  {
   Part=Group(Class,Slot);const FString Owner=Class+TEXT("Studio"),Key=Part+TEXT("_")+Original->GetName();
   const FString Path=TEXT("/Game/FPS/Customizable/Character/Materials/")+Owner+TEXT("/MI_")+Owner+TEXT("_")+Key+TEXT("_Color");
   Replacement=LoadObject<UMaterialInterface>(nullptr,*Path,nullptr,LOAD_NoWarn);
   if(!Replacement)Replacement=ClothingMaterial(Original,Owner,Key);++Outfits;OutfitBodies.Add(Mesh);
  }
  else if(Original->GetName().StartsWith(TEXT("MI_EyeL_"))||Original->GetName().StartsWith(TEXT("MI_EyeR_")))
  {
   const FString Body=Mesh->GetName().Mid(4).LeftChop(9);
   const FString Side=Original->GetName().StartsWith(TEXT("MI_EyeL_"))?TEXT("L"):TEXT("R");
   Replacement=LoadObject<UMaterialInterface>(nullptr,*(TEXT("/Game/FPS/Customizable/Character/Materials/ClassEyes/MI_")+Body+TEXT("_Eye")+Side));check(Replacement);++Eyes;EyeBodies.Add(Mesh);
  }
  if(!Replacement)continue;
  TArray<TPair<FName,UEdGraphPin*>> Links;for(auto* Pin:N->Pins)for(auto* Other:Pin->LinkedTo)Links.Add({Pin->PinName,Other});
  Set(N,TEXT("Material"),Replacement->GetPathName());
  for(const auto& Saved:Links)if(auto* Pin=N->FindPin(Saved.Key))if(!Pin->LinkedTo.Contains(Saved.Value))
   {if(Pin->Direction==EGPD_Output)Link(G,N,Saved.Key,Saved.Value->GetOwningNode(),Saved.Value->PinName);else Link(G,Saved.Value->GetOwningNode(),Saved.Value->PinName,N,Saved.Key);}
  auto Connect=[&](const TCHAR* Pin,const FString& Name,const TCHAR* Output){check(N->FindPin(Pin));if(!N->FindPin(Pin)->LinkedTo.Contains(Parameters.FindChecked(Name)->FindPin(Output)))Link(G,Parameters.FindChecked(Name),Output,N,Pin);};
  if(!Part.IsEmpty()){Connect(TEXT("ClassPartTint"),Part+TEXT("Color"),TEXT("Color"));Connect(TEXT("ClassPartValue"),Part+TEXT("Value"),TEXT("Float"));Connect(TEXT("ClassUseOriginal"),Part+TEXT("UseOriginal"),TEXT("Float"));}
  else Connect(TEXT("Iris Color Multiply"),TEXT("EyeColor"),TEXT("Color"));
 }
 checkf(EyeBodies.Num()==3&&OutfitBodies.Num()==3,TEXT("All three body branches are required; rebuild with FPS.ClassVariants.Build"));G->NotifyGraphChanged();CO->PostEditChange();
 UE_LOG(LogTemp,Display,TEXT("CLASS_COLORS_CONNECTED %s eyes=%d outfits=%d"),*Class,Eyes,Outfits);
}
static FAutoConsoleCommand Command(TEXT("FPS.ClassColors.Apply"),TEXT("Connect private class colors; requires prepared eye instances"),FConsoleCommandDelegate::CreateLambda([]()
{
 for(const TCHAR* Class:{TEXT("Trooper"),TEXT("Vanguard")})
 {auto* CO=LoadObject<UCustomizableObject>(nullptr,*(FString(TEXT("/Game/FPS/Customizable/CO_"))+Class));check(CO);CO->AddToRoot();Apply(CO);FCompileParams P;P.bAsync=false;P.bSkipIfNotOutOfDate=false;CO->Compile(P);check(CO->IsCompiled());ClassCO::Save(CO);CO->RemoveFromRoot();}
}));
}
