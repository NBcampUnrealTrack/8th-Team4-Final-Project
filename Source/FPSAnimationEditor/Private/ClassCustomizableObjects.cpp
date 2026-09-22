#include "HAL/IConsoleManager.h"
#include "MuCO/CustomizableObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"
#include "EdGraph/EdGraphSchema.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "Commandlets/Commandlet.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"

namespace ClassCO
{
UEdGraph* Graph(UCustomizableObject* CO)
{
 auto* P=FindFProperty<FObjectPropertyBase>(CO->GetClass(),TEXT("Source"));
 return P?Cast<UEdGraph>(P->GetObjectPropertyValue_InContainer(CO)):nullptr;
}
FString Property(UObject* Object,const TCHAR* Name)
{
 FString Text;if(auto* P=Object->GetClass()->FindPropertyByName(Name))P->ExportText_InContainer(0,Text,Object,nullptr,Object,PPF_None);
 return Text;
}
void Inspect()
{
 auto* CO=LoadObject<UCustomizableObject>(nullptr,TEXT("/Game/FPS/Customizable/CO_Ranger"));if(!CO)return;
 auto* G=Graph(CO);if(!G)return;
 UE_LOG(LogTemp,Display,TEXT("CLASS_CO_INSPECT nodes=%d"),G->Nodes.Num());
 for(UEdGraphNode* N:G->Nodes)
 {
  FString Properties;
  for(const TCHAR* P:{TEXT("SkeletalMesh"),TEXT("Material"),TEXT("ReferenceSkeletalMesh"),TEXT("ComponentName"),TEXT("ParameterName"),TEXT("Values"),TEXT("DefaultIndex"),TEXT("ObjectName"),TEXT("NumLODs")})
  {const FString V=Property(N,P);if(!V.IsEmpty())Properties+=FString(P)+TEXT("=")+V+TEXT(" ");}
  UE_LOG(LogTemp,Display,TEXT("CO_NODE %s %s %s"),*N->GetName(),*N->GetClass()->GetName(),*Properties);
  for(auto* P:N->Pins)
  {
   FString Links;for(auto* L:P->LinkedTo)Links+=L->GetOwningNode()->GetName()+TEXT(".")+L->PinName.ToString()+TEXT(";");
   if(!Links.IsEmpty())UE_LOG(LogTemp,Display,TEXT("CO_PIN %s %d %s [%s] -> %s"),*N->GetName(),int32(P->Direction),*P->PinName.ToString(),*P->PinType.PinCategory.ToString(),*Links);
  }
 }
 for(const TCHAR* Path:{TEXT("/Game/MetaHumans/Mason/Body/SKM_Mason_BodyMesh"),TEXT("/Game/MetaHumans/Mason/Face/SKM_Mason_FaceMesh"),TEXT("/Game/MetaHumans/Mason/Clothing/Mason_Outfits"),TEXT("/Game/CharacterAssemblies/MHC_Shrank/Body/SKM_MHC_Shrank_BodyMesh"),TEXT("/Game/CharacterAssemblies/MHC_Shrank/Face/SKM_MHC_Shrank_FaceMesh"),TEXT("/Game/CharacterAssemblies/MHC_Shrank/Clothing/MHC_Shrank_Outfits")})
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,Path);if(!Mesh){UE_LOG(LogTemp,Error,TEXT("CO_MESH missing %s"),Path);continue;}
  UE_LOG(LogTemp,Display,TEXT("CO_MESH %s lods=%d materials=%d"),Path,Mesh->GetLODNum(),Mesh->GetMaterials().Num());
  int32 I=0;for(const auto& M:Mesh->GetMaterials())UE_LOG(LogTemp,Display,TEXT("CO_SLOT %d %s %s"),I++,*M.MaterialSlotName.ToString(),*GetPathNameSafe(M.MaterialInterface));
 }
}
static FAutoConsoleCommand InspectCommand(TEXT("FPS.ClassCO.Inspect"),TEXT("Read-only source graph and target meshes"),FConsoleCommandDelegate::CreateStatic(&Inspect));
static void InspectShrank()
{
 UObject* A=LoadObject<UObject>(nullptr,TEXT("/Game/Fab/MetaHuman/MHC_Shrank.MHC_Shrank"));check(A);
 TArray<UObject*> Objects;GetObjectsWithOuter(A,Objects,true);Objects.Insert(A,0);
 for(UObject* O:Objects)
 {
  if(O->GetClass()->GetName().Contains(TEXT("Collection"))||O->GetClass()->GetName().Contains(TEXT("Instance")))
  for(TFieldIterator<FProperty> P(O->GetClass());P;++P)
  { FString V;P->ExportText_InContainer(0,V,O,nullptr,O,PPF_None);UE_LOG(LogTemp,Display,TEXT("SHRANK_DATA %s %s=%s"),*O->GetName(),*P->GetName(),*V.Left(30000)); }
 }
}
static FAutoConsoleCommand InspectShrankCommand(TEXT("FPS.ClassCO.InspectShrank"),TEXT("Inspect Shrank clothing state"),FConsoleCommandDelegate::CreateStatic(&InspectShrank));
static void RestoreShrank(const TArray<FString>& Args)
{
 UObject* A=LoadObject<UObject>(nullptr,TEXT("/Game/Fab/MetaHuman/MHC_Shrank.MHC_Shrank"));check(A);
 auto* CP=FindFProperty<FObjectPropertyBase>(A->GetClass(),TEXT("InternalCollection"));check(CP);
 UObject* C=CP->GetObjectPropertyValue_InContainer(A);
 auto* IP=FindFProperty<FObjectPropertyBase>(C->GetClass(),TEXT("DefaultInstance"));check(IP);
 UObject* I=IP->GetObjectPropertyValue_InContainer(C);
 auto* P=I->GetClass()->FindPropertyByName(TEXT("SlotSelections"));check(P);
 FString Before;P->ExportText_InContainer(0,Before,I,nullptr,I,PPF_None);
 const FString Old=TEXT("/Game/Fab/Mercenary/WI_OA_outfit_64.WI_OA_outfit_64");
 const FString Basic=TEXT("/MetaHumanCharacter/Optional/Clothing/WI_DefaultGarment.WI_DefaultGarment");
 if(Args.Num()==1&&Args[0]==TEXT("apply"))
 {
  check(Before.Contains(Old)&&!Before.Contains(Basic));
  check(LoadObject<UObject>(nullptr,*Basic));
  const FString After=Before.Replace(*Old,*Basic,ESearchCase::CaseSensitive);
  check(P->ImportText_InContainer(*After,I,I,PPF_None));
  FString Actual;P->ExportText_InContainer(0,Actual,I,nullptr,I,PPF_None);check(Actual==After);
  A->MarkPackageDirty();FSavePackageArgs SaveArgs;SaveArgs.TopLevelFlags=RF_Public|RF_Standalone;
  check(UPackage::SavePackage(A->GetOutermost(),A,*FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),SaveArgs));
  UE_LOG(LogTemp,Display,TEXT("SHRANK_BASIC_SAVED: only Outfits selection replaced; remaining selections unchanged"));
 }
 else
 {
  const bool Valid=!Before.Contains(Old)&&Before.Contains(Basic);
  if(Valid) { UE_LOG(LogTemp,Display,TEXT("SHRANK_BASIC_VERIFIED %s"),*Before); }
  else { UE_LOG(LogTemp,Error,TEXT("SHRANK_BASIC_FAILED: outfit selection still points to non-default clothing")); }
 }
}
static FAutoConsoleCommand RestoreShrankCommand(TEXT("FPS.ClassCO.RestoreShrank"),TEXT("Verify basic outfit; explicit apply replaces only Mercenary selection"),FConsoleCommandWithArgsDelegate::CreateStatic(&RestoreShrank));

struct FClassSource
{
 const TCHAR* Name;
 const TCHAR* Meshes[3]; // Body, Face, Outfit
};
static const FClassSource Sources[] = {
 {TEXT("Trooper"), {TEXT("/Game/MetaHumans/Mason/Body/SKM_Mason_BodyMesh"), TEXT("/Game/MetaHumans/Mason/Face/SKM_Mason_FaceMesh"), TEXT("/Game/MetaHumans/Mason/Clothing/Mason_Outfits")}},
 {TEXT("Vanguard"), {TEXT("/Game/CharacterAssemblies/MHC_Shrank/Body/SKM_MHC_Shrank_BodyMesh"), TEXT("/Game/CharacterAssemblies/MHC_Shrank/Face/SKM_MHC_Shrank_FaceMesh"), TEXT("/Game/CharacterAssemblies/MHC_Shrank/Clothing/MHC_Shrank_Outfits")}}
};
static const TCHAR* Components[] = {TEXT("Body"), TEXT("Face"), TEXT("Outfit")};
static TAutoConsoleVariable<int32> TestFailures(TEXT("FPS.ClassCO.TestFailures"), -1,
 TEXT("Result of the last complete class CO verification; -1 means not completed."));

void Set(UObject* Object, const TCHAR* Name, const FString& Value)
{
 FProperty* P = Object->GetClass()->FindPropertyByName(Name);
 checkf(P, TEXT("Missing property %s on %s"), Name, *Object->GetClass()->GetName());
 checkf(P->ImportText_InContainer(*Value, Object, Object, PPF_None), TEXT("Invalid %s=%s"), Name, *Value);
 FPropertyChangedEvent Event(P);
 Object->PostEditChangeProperty(Event);
}

void Save(UObject* Object)
{
 Object->MarkPackageDirty();
 FSavePackageArgs Args;
 Args.TopLevelFlags = RF_Public | RF_Standalone;
 check(UPackage::SavePackage(Object->GetOutermost(), Object,
  *FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args));
}

UEdGraphNode* Node(UEdGraph* G, const TCHAR* Type, int32 X, int32 Y)
{
 UClass* C = LoadClass<UEdGraphNode>(nullptr, *(FString(TEXT("/Script/CustomizableObjectEditor.CustomizableObjectNode")) + Type));
 check(C);
 auto* N = NewObject<UEdGraphNode>(G, C, NAME_None, RF_Transactional);
 G->AddNode(N, false, false);
 N->CreateNewGuid(); N->PostPlacedNewNode(); N->AllocateDefaultPins();
 N->NodePosX = X; N->NodePosY = Y;
 return N;
}

void Link(UEdGraph* G, UEdGraphNode* A, FName Out, UEdGraphNode* B, FName In)
{
 auto* P = A->FindPin(Out); auto* Q = B->FindPin(In);
 checkf(P && Q, TEXT("Missing pins %s.%s -> %s.%s"), *A->GetName(), *Out.ToString(), *B->GetName(), *In.ToString());
 P->bHidden = false; Q->bHidden = false;
 check(G->GetSchema()->TryCreateConnection(P, Q));
}

template<class T> static T* Expression(UMaterial* M)
{
 T* E = NewObject<T>(M);
 M->GetExpressionCollection().AddExpression(E);
 return E;
}

// New, class-owned materials only. Defaults preserve the original appearance.
UMaterialInterface* ClothingMaterial(UMaterialInterface* Source, const FString& Class, const FString& Group)
{
 const FString Folder = TEXT("/Game/FPS/Customizable/Character/Materials/") + Class + TEXT("/");
 const FString Name = TEXT("M_") + Class + TEXT("_") + Group + TEXT("_Color");
 const FString Path = Folder + Name;
 checkf(!FPackageName::DoesPackageExist(Path), TEXT("Refusing to overwrite %s"), *Path);
 auto* M = DuplicateObject<UMaterial>(Source->GetMaterial(), CreatePackage(*Path), *Name);
 M->SetFlags(RF_Public | RF_Standalone);
 auto* Data = M->GetEditorOnlyData();
 FExpressionInput Original;
 if (M->bUseMaterialAttributes)
 {
  auto* Get = Expression<UMaterialExpressionGetMaterialAttributes>(M);
  Get->MaterialAttributes = Data->MaterialAttributes;
  Original.Expression = Get;
  Original.OutputIndex = Get->CreateOrGetOutputAttribute(MP_BaseColor);
 }
 else if (Data->BaseColor.Expression)
 {
  Original = Data->BaseColor;
 }
 else
 {
  auto* Constant = Expression<UMaterialExpressionConstant3Vector>(M);
  Constant->Constant = FLinearColor(Data->BaseColor.Constant);
  Original.Expression = Constant;
 }
 auto* Color = Expression<UMaterialExpressionVectorParameter>(M);
 Color->ParameterName = TEXT("ClassPartTint"); Color->DefaultValue = FLinearColor::White;
 Color->UpdateParameterGuid(true, true);
 auto MakeScalar = [M](FName Name, float Max)
 {
  auto* V = Expression<UMaterialExpressionScalarParameter>(M);
  V->ParameterName = Name; V->DefaultValue = 1.f; V->SliderMin = 0.f; V->SliderMax = Max;
  V->UpdateParameterGuid(true, true); return V;
 };
 auto* Value = MakeScalar(TEXT("ClassPartValue"), 2.f);
 auto* UseOriginal = MakeScalar(TEXT("ClassUseOriginal"), 1.f);
 auto* Recolor = Expression<UMaterialExpressionCustom>(M);
 Recolor->Description = TEXT("Class clothing recolor; 1 = original, 0 = tint; other material properties unchanged");
 Recolor->OutputType = CMOT_Float3;
 Recolor->Code = TEXT("float detail = lerp(0.65, 1.0, saturate(dot(OriginalRGB, float3(0.2126, 0.7152, 0.0722)))); return lerp(TargetRGB * detail, OriginalRGB, saturate(UseOriginal)) * max(Value, 0.0);");
 Recolor->Inputs.Empty();
 FCustomInput A; A.InputName = TEXT("OriginalRGB"); A.Input = Original; Recolor->Inputs.Add(A);
 for (auto Pair : {TPair<FName, UMaterialExpression*>(TEXT("TargetRGB"), Color), {TEXT("UseOriginal"), UseOriginal}, {TEXT("Value"), Value}})
 {
  FCustomInput Input; Input.InputName = Pair.Key; Input.Input.Expression = Pair.Value; Recolor->Inputs.Add(Input);
 }
 if (M->bUseMaterialAttributes)
 {
  auto* SetAttributes = Expression<UMaterialExpressionSetMaterialAttributes>(M);
  SetAttributes->Inputs[0] = Data->MaterialAttributes;
  check(SetAttributes->ConnectInputAttribute(MP_BaseColor, Recolor));
  Data->MaterialAttributes.Connect(0, SetAttributes);
 }
 else
 {
  Data->BaseColor.Connect(0, Recolor);
 }
 M->PostEditChange(); FAssetRegistryModule::AssetCreated(M); Save(M);
 const FString InstanceName = TEXT("MI_") + Class + TEXT("_") + Group + TEXT("_Color");
 check(!FPackageName::DoesPackageExist(Folder + InstanceName));
 auto* MI = NewObject<UMaterialInstanceConstant>(CreatePackage(*(Folder + InstanceName)), *InstanceName, RF_Public | RF_Standalone);
 MI->SetParentEditorOnly(M);
 MI->CopyMaterialUniformParametersEditorOnly(Source, true);
 MI->PostEditChange(); FAssetRegistryModule::AssetCreated(MI); Save(MI);
 return MI;
}

static void Create()
{
 // Refuse partial replacement: this command is for first-time creation only.
 for (const auto& Source : Sources)
 {
  TArray<FString> Packages = {FString(TEXT("/Game/FPS/Customizable/CO_")) + Source.Name};
  const TArray<FString> Groups = FString(Source.Name) == TEXT("Trooper") ?
   TArray<FString>{TEXT("Bottom")} : TArray<FString>{TEXT("Scarf"), TEXT("Top"), TEXT("Boots")};
  for (const FString& Group : Groups) for (const TCHAR* Prefix : {TEXT("M_"), TEXT("MI_")})
   Packages.Add(FString(TEXT("/Game/FPS/Customizable/Character/Materials/")) + Source.Name + TEXT("/") + Prefix + Source.Name + TEXT("_") + Group + TEXT("_Color"));
  for (const FString& Package : Packages) if (FPackageName::DoesPackageExist(Package) || FindPackage(nullptr, *Package))
  { UE_LOG(LogTemp, Error, TEXT("CLASS_CO_CREATE_REFUSED: existing package %s"), *Package); return; }
 }
 auto* Template = LoadObject<UCustomizableObject>(nullptr, TEXT("/Game/FPS/Customizable/CO_Ranger"));
 check(Template && Graph(Template));
 for (const auto& Source : Sources)
 {
  const FString Name = FString(TEXT("CO_")) + Source.Name;
  auto* CO = DuplicateObject<UCustomizableObject>(Template, CreatePackage(*(TEXT("/Game/FPS/Customizable/") + Name)), *Name);
  CO->SetFlags(RF_Public | RF_Standalone); CO->AddToRoot();
  auto* G = Graph(CO); check(G && G != Graph(Template));
  TMap<FString, UEdGraphNode*> Parameters;
  UEdGraphNode* Root = nullptr;
  const auto OldNodes = G->Nodes;
  for (UEdGraphNode* N : OldNodes)
  {
   for (auto* P : N->Pins) P->BreakAllPinLinks();
   if (N->GetClass()->GetName() == TEXT("CustomizableObjectNodeObject")) { Root = N; continue; }
   if (auto* P = FindFProperty<FStrProperty>(N->GetClass(), TEXT("ParameterName")))
   { Parameters.Add(P->GetPropertyValue_InContainer(N), N); continue; }
   G->RemoveNode(N);
  }
  check(Root); Root->NodePosX = 1600; Root->NodePosY = 0;
  auto* BodyType = Parameters.FindChecked(TEXT("BodyType"));
  Set(BodyType, TEXT("Values"), TEXT("((Name=\"Base\"))"));
  BodyType->NodePosX = -1400; BodyType->NodePosY = -400;
  TSet<FString> UsedParameters = {TEXT("BodyType")};
  for (int32 Part = 0; Part < 3; ++Part)
  {
   auto* Mesh = LoadObject<USkeletalMesh>(nullptr, Source.Meshes[Part]); check(Mesh);
   auto* Component = Node(G, TEXT("ComponentMesh"), 1200, Part * 2800);
   Set(Component, TEXT("ComponentName"), Components[Part]);
   Set(Component, TEXT("ReferenceSkeletalMesh"), Mesh->GetPathName());
   Set(Component, TEXT("NumLODs"), FString::FromInt(Mesh->GetLODNum()));
   Set(Component, TEXT("AutoLODStrategy"), TEXT("Manual"));
   Link(G, Component, TEXT("Component"), Root, TEXT("Components"));
   auto* MeshNode = Node(G, TEXT("SkeletalMesh"), -1000, Part * 2800);
   Set(MeshNode, TEXT("SkeletalMesh"), Mesh->GetPathName());
   TMap<UMaterialInterface*, UMaterialInterface*> Recolored;
   const auto* Imported = Mesh->GetImportedModel(); check(Imported);
   int32 Row = 0;
   for (int32 LOD = 0; LOD < Imported->LODModels.Num(); ++LOD)
   {
    const auto& Sections = Imported->LODModels[LOD].Sections;
    for (int32 Section = 0; Section < Sections.Num(); ++Section)
    {
     const int32 MaterialIndex = Sections[Section].MaterialIndex;
     UMaterialInterface* Material = Mesh->GetMaterials()[MaterialIndex].MaterialInterface; check(Material);
     FString Group;
     if (Part == 2)
     {
      Group = FString(Source.Name) == TEXT("Trooper") ? TEXT("Bottom") :
       MaterialIndex == 0 ? TEXT("Scarf") : MaterialIndex == 1 ? TEXT("Top") : TEXT("Boots");
      if (!Recolored.Contains(Material)) Recolored.Add(Material, ClothingMaterial(Material, Source.Name, Group));
      Material = Recolored.FindChecked(Material);
     }
     auto* MatNode = Node(G, TEXT("Material"), -300, Part * 2800 + Row * 180);
     Set(MatNode, TEXT("Material"), Material->GetPathName());
     Link(G, MeshNode, *FString::Printf(TEXT("LOD %d - Section %d - Mesh"), LOD, Section), MatNode, TEXT("Mesh_Input_Pin"));
     auto ConnectParameter = [&](const TCHAR* Pin, const FString& Parameter, const TCHAR* Output)
     {
      if (MatNode->FindPin(Pin))
      { Link(G, Parameters.FindChecked(Parameter), Output, MatNode, Pin); UsedParameters.Add(Parameter); }
     };
     ConnectParameter(TEXT("Basecolor Global Multiply Post-Bake"), TEXT("SkinColor"), TEXT("Color"));
     ConnectParameter(TEXT("Basecolor Global Value Post-Bake"), TEXT("SkinValue"), TEXT("Float"));
     ConnectParameter(TEXT("Iris Color Multiply"), TEXT("EyeColor"), TEXT("Color"));
     if (!Group.IsEmpty())
     {
      ConnectParameter(TEXT("ClassPartTint"), Group + TEXT("Color"), TEXT("Color"));
      ConnectParameter(TEXT("ClassPartValue"), Group + TEXT("Value"), TEXT("Float"));
      ConnectParameter(TEXT("ClassUseOriginal"), Group + TEXT("UseOriginal"), TEXT("Float"));
     }
     auto* Switch = Node(G, TEXT("MaterialSwitch"), 500, Part * 2800 + Row * 180);
     Link(G, BodyType, TEXT("Enum"), Switch, TEXT("Switch Parameter"));
     Link(G, MatNode, TEXT("Mesh Section_Output_Pin"), Switch, TEXT("Material 0 "));
     Link(G, Switch, TEXT("Material"), Component, *FString::Printf(TEXT("LOD %d"), LOD));
     ++Row;
    }
   }
  }
  int32 ParameterRow = 0;
  for (const auto& Pair : Parameters)
  {
   if (!UsedParameters.Contains(Pair.Key)) G->RemoveNode(Pair.Value);
   else if (Pair.Value != BodyType)
   { Pair.Value->NodePosX = -1800; Pair.Value->NodePosY = ParameterRow++ * 200; }
  }
  G->NotifyGraphChanged(); CO->PostEditChange(); FAssetRegistryModule::AssetCreated(CO); Save(CO);
  CO->RemoveFromRoot();
  UE_LOG(LogTemp, Display, TEXT("CLASS_CO_CREATED %s"), *CO->GetPathName());
 }
}

static void Test()
{
 TestFailures->Set(-1);
 int32 Failures = 0;
 for (const auto& Source : Sources)
 {
  auto* CO = LoadObject<UCustomizableObject>(nullptr, *(FString(TEXT("/Game/FPS/Customizable/CO_")) + Source.Name));
  if (!CO || !Graph(CO)) { ++Failures; continue; }
  bool Valid = true;
  int32 ComponentCount = 0;
  for (UEdGraphNode* N : Graph(CO)->Nodes)
  {
   if (N->GetClass()->GetName() == TEXT("CustomizableObjectNodeComponentMesh"))
   {
    ++ComponentCount;
    const FString Component = Property(N, TEXT("ComponentName"));
    for (int32 Part = 0; Part < 3; ++Part) if (Component == Components[Part])
     Valid &= Property(N, TEXT("ReferenceSkeletalMesh")).Contains(Source.Meshes[Part]);
   }
   if (Property(N, TEXT("ParameterName")) == TEXT("BodyType"))
    Valid &= Property(N, TEXT("Values")) == TEXT("((Name=\"Base\"))");
  }
  Valid &= ComponentCount == 3;
  CO->AddToRoot();
  FCompileParams Params; Params.bAsync = false; Params.bSkipIfNotOutOfDate = false; CO->Compile(Params);
  auto* Instance = CO->CreateInstance(); check(Instance); Instance->AddToRoot();
  Instance->SetIntParameterSelectedOption(TEXT("BodyType"), TEXT("Base"));
  auto Done = MakeShared<bool>(false); auto Result = MakeShared<EUpdateResult>(EUpdateResult::Error);
  FInstanceUpdateNativeDelegate Delegate;
  Delegate.AddLambda([Done, Result](const FUpdateContext& Context) { *Done = true; *Result = Context.UpdateResult; });
  Instance->UpdateSkeletalMeshAsyncResult(Delegate, true, true);
  const double End = FPlatformTime::Seconds() + 180.;
  while (!*Done && FPlatformTime::Seconds() < End && !IsEngineExitRequested()) CommandletHelpers::TickEngine();
  Valid &= *Done && (*Result == EUpdateResult::Success || *Result == EUpdateResult::Warning);
  for (int32 Part = 0; Part < 3; ++Part)
  {
   auto* Mesh = Instance->GetSkeletalMeshComponentSkeletalMesh(Components[Part]);
   auto* Render = Mesh ? Mesh->GetResourceForRendering() : nullptr;
   auto* Original = LoadObject<USkeletalMesh>(nullptr, Source.Meshes[Part]);
   bool MeshValid = Render && Render->LODRenderData.Num() == Original->GetLODNum();
   if (Render) for (const auto& LOD : Render->LODRenderData) MeshValid &= LOD.GetNumVertices() > 0 && LOD.RenderSections.Num() > 0;
   Valid &= MeshValid;
   UE_LOG(LogTemp, Display, TEXT("CLASS_CO_MESH %s %s lods=%d valid=%d"), Source.Name, Components[Part], Render ? Render->LODRenderData.Num() : 0, MeshValid);
  }
  // Exercise exposed controls on generated material instances, not source text.
  const TArray<FString> Groups = FString(Source.Name) == TEXT("Trooper") ?
   TArray<FString>{TEXT("Bottom")} : TArray<FString>{TEXT("Scarf"), TEXT("Top"), TEXT("Boots")};
  for (int32 Pass = 0; *Done && Pass < 2; ++Pass)
  {
   const FLinearColor Color(.18f, .47f, .73f, 1.f);
   const float Brightness = Pass == 0 ? 1.35f : 1.f;
   const float Original = Pass == 0 ? 0.f : 1.f;
   for (const FString& Group : Groups)
   {
    Instance->SetColorParameterSelectedOption(Group + TEXT("Color"), Color);
    Instance->SetFloatParameterSelectedOption(Group + TEXT("Value"), Brightness);
    Instance->SetFloatParameterSelectedOption(Group + TEXT("UseOriginal"), Original);
   }
   Instance->SetColorParameterSelectedOption(TEXT("SkinColor"), Color);
   Instance->SetFloatParameterSelectedOption(TEXT("SkinValue"), Brightness);
   Instance->SetColorParameterSelectedOption(TEXT("EyeColor"), Color);
   *Done = false;
   Instance->UpdateSkeletalMeshAsyncResult(Delegate, true, true);
   const double ColorEnd = FPlatformTime::Seconds() + 180.;
   while (!*Done && FPlatformTime::Seconds() < ColorEnd && !IsEngineExitRequested()) CommandletHelpers::TickEngine();
   bool ColorsValid = *Done && (*Result == EUpdateResult::Success || *Result == EUpdateResult::Warning);
   TSet<FString> FoundGroups;
   int32 SkinCount = 0, EyeCount = 0;
   for (const TCHAR* Component : Components)
   {
    auto* Mesh = Instance->GetSkeletalMeshComponentSkeletalMesh(Component);
    if (!Mesh) { ColorsValid = false; continue; }
    for (const auto& Slot : Mesh->GetMaterials())
    {
     UMaterialInterface* MI = Slot.MaterialInterface;
     if (!MI) { ColorsValid = false; continue; }
     FLinearColor Actual; float ActualValue = 0.f, ActualOriginal = 0.f;
     if (MI->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Basecolor Global Multiply Post-Bake")), Actual))
     {
      ++SkinCount; ColorsValid &= Actual.Equals(Color, .001f);
      ColorsValid &= MI->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Basecolor Global Value Post-Bake")), ActualValue) && FMath::IsNearlyEqual(ActualValue, Brightness);
     }
     if (MI->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Iris Color Multiply")), Actual))
     { ++EyeCount; ColorsValid &= Actual.Equals(Color, .001f); }
     if (MI->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ClassPartTint")), Actual))
     {
      ColorsValid &= Actual.Equals(Color, .001f);
      ColorsValid &= MI->GetScalarParameterValue(FMaterialParameterInfo(TEXT("ClassPartValue")), ActualValue) && FMath::IsNearlyEqual(ActualValue, Brightness);
      ColorsValid &= MI->GetScalarParameterValue(FMaterialParameterInfo(TEXT("ClassUseOriginal")), ActualOriginal) && FMath::IsNearlyEqual(ActualOriginal, Original);
      for (const FString& Group : Groups) if (MI->GetMaterial()->GetName().Contains(TEXT("_") + Group + TEXT("_"))) FoundGroups.Add(Group);
     }
    }
   }
   ColorsValid &= FoundGroups.Num() == Groups.Num() && SkinCount >= 2 && EyeCount >= 2;
   Valid &= ColorsValid;
   UE_LOG(LogTemp, Display, TEXT("CLASS_CO_COLORS %s pass=%d groups=%d skin=%d eyes=%d valid=%d"), Source.Name, Pass, FoundGroups.Num(), SkinCount, EyeCount, ColorsValid);
  }
  if (!Valid) ++Failures;
  UE_LOG(LogTemp, Display, TEXT("CLASS_CO_INSTANCE %s done=%d result=%d valid=%d"), Source.Name, *Done, int32(*Result), Valid);
  if (*Done) { Instance->RemoveFromRoot(); CO->RemoveFromRoot(); }
 }
 UE_LOG(LogTemp, Display, TEXT("CLASS_CO_TEST failures=%d"), Failures);
 TestFailures->Set(Failures);
 if (Failures) UE_LOG(LogTemp, Error, TEXT("CLASS_CO_TEST_FAILED"));
}

static FAutoConsoleCommand CreateCommand(TEXT("FPS.ClassCO.Create"), TEXT("Create new class COs from Ranger; refuses existing assets"), FConsoleCommandDelegate::CreateStatic(&Create));
static FAutoConsoleCommand TestCommand(TEXT("FPS.ClassCO.Test"), TEXT("Compile and generate both class COs without saving"), FConsoleCommandDelegate::CreateStatic(&Test));
}
