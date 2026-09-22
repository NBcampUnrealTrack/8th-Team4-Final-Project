#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosOutfitAsset/Outfit.h"
#include "ChaosOutfitAsset/OutfitAsset.h"
#include "ChaosOutfitAsset/SizedOutfitSource.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MetaHumanWardrobeItem.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCharacterInstance.h"
#include "Item/MetaHumanOutfitPipeline.h"
#include "Item/MetaHumanOutfitEditorPipeline.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "Editor.h"
#include "Commandlets/Commandlet.h"
#include "Rendering/SkeletalMeshRenderData.h"

namespace WardrobeNoSunglasses
{
static void InspectSlots()
{
 for(const TCHAR* Name:{TEXT("Dean"),TEXT("MHC_Crowd80"),TEXT("MHC_Shrank")})
 {
  auto* A=LoadObject<UMetaHumanCharacter>(nullptr,*(FString(TEXT("/Game/Fab/MetaHuman/"))+Name));check(A);
  const auto* P=A->GetMutableInternalCollection()->GetPipeline();
  const auto S=P->GetSpecification();
  UE_LOG(LogTemp,Display,TEXT("SLOT_SPEC %s count=%d find_hair=%d find_outfits=%d"),Name,S->Slots.Num(),S->Slots.Contains(TEXT("Hair")),S->Slots.Contains(TEXT("Outfits")));
  for(const auto& Pair:S->Slots)UE_LOG(LogTemp,Display,TEXT("SLOT_ENTRY %s %s target=%s types=%d"),Name,*Pair.Key.ToString(),*Pair.Value.TargetSlot.ToString(),Pair.Value.SupportedPrincipalAssetTypes.Num());
 }
}
static FAutoConsoleCommand InspectSlotsCommand(TEXT("FPS.Wardrobe.InspectSlots"),TEXT("Read saved slot definitions"),FConsoleCommandDelegate::CreateStatic(&InspectSlots));
static const FString Folder=TEXT("/Game/Fab/Mercenary/NoSunglasses/");
static const TCHAR* Characters[]={TEXT("MHC_Crowd80"),TEXT("MHC_Shrank"),TEXT("Dean")};
static bool IsGlasses(const FString& Path)
{
 return Path==TEXT("/Game/Fab/Mercenary/outfit_64/Materials/Frame.Frame")||Path==TEXT("/Game/Fab/Mercenary/outfit_64/Materials/Frame1.Frame1");
}
static void Save(UObject* O)
{
 O->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 check(UPackage::SavePackage(O->GetOutermost(),O,*FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
}
static UMetaHumanCharacterInstance* Instance(UMetaHumanCollection* Collection)
{
 auto* P=FindFProperty<FObjectPropertyBase>(Collection->GetClass(),TEXT("DefaultInstance"));check(P);
 return CastChecked<UMetaHumanCharacterInstance>(P->GetObjectPropertyValue_InContainer(Collection));
}
static void Create()
{
 for(const TCHAR* Name:{TEXT("CA_a_NoSunglasses"),TEXT("CA_b_NoSunglasses"),TEXT("OA_outfit_64_NoSunglasses"),TEXT("WI_OA_outfit_64_NoSunglasses")})
 {
  if(FPackageName::DoesPackageExist(Folder+Name)||FindPackage(nullptr,*(Folder+Name)))
  { UE_LOG(LogTemp,Error,TEXT("NO_GLASSES_REFUSED existing %s"),Name);return; }
 }
 // Load all targets before authoring any assets. No generic reset of their appearance.
 TArray<UMetaHumanCharacter*> Targets;
 for(const TCHAR* Name:Characters)
 {
  auto* A=LoadObject<UMetaHumanCharacter>(nullptr,*(FString(TEXT("/Game/Fab/MetaHuman/"))+Name));
  check(A&&A->GetMutableInternalCollection());Targets.Add(A);A->AddToRoot();
 }
 auto* Outfit=NewObject<UChaosOutfit>();Outfit->AddToRoot();
 const FGuid OutfitGuid=FGuid::NewGuid();
 for(const TCHAR* Size:{TEXT("a"),TEXT("b")})
 {
  const auto* Source=LoadObject<UChaosClothAsset>(nullptr,*(FString(TEXT("/Game/Fab/Mercenary/outfit_64/ClothAssets/CA_"))+Size));check(Source);
  const FString Name=FString(TEXT("CA_"))+Size+TEXT("_NoSunglasses");
  auto* Copy=DuplicateObject<UChaosClothAsset>(Source,CreatePackage(*(Folder+Name)),*Name);Copy->SetFlags(RF_Public|RF_Standalone);Copy->AddToRoot();
  // A baked derivative must not reevaluate the original graph and restore glasses.
  Copy->SetDataflow(nullptr);
  TArray<TSharedRef<const FManagedArrayCollection>> Collections;
  for(const auto& SourceCollection:Source->GetClothCollections())
  {
   auto Data=MakeShared<FManagedArrayCollection>(*SourceCollection);
   UE::Chaos::ClothAsset::FCollectionClothFacade Cloth(Data);
   TArray<int32> Remove;
   for(int32 Index=0;Index<Cloth.GetNumRenderPatterns();++Index)
    if(IsGlasses(Cloth.GetRenderPattern(Index).GetRenderMaterialSoftObjectPathName().ToString()))Remove.Add(Index);
   Cloth.RemoveRenderPatterns(Remove);
   check(Cloth.GetNumRenderPatterns()==5);
   UE_LOG(LogTemp,Display,TEXT("NO_GLASSES_FILTER size=%s removed=%d remaining=%d"),Size,Remove.Num(),Cloth.GetNumRenderPatterns());
   Collections.Add(Data);
  }
  FText Error,Verbose;Copy->Build(Collections,nullptr,&Error,&Verbose);checkf(Error.IsEmpty(),TEXT("Cloth build failed: %s %s"),*Error.ToString(),*Verbose.ToString());
  FAssetRegistryModule::AssetCreated(Copy);Save(Copy);
  const FString BodyPath=FString(TEXT("/Game/Fab/Mercenary/outfit_64/Meshes/SM_body_"))+Size+TEXT("_CombinedSkelMesh_body_")+Size+TEXT("_CombinedSkelMesh");
  auto* Body=LoadObject<USkeletalMesh>(nullptr,*BodyPath);check(Body);
  FChaosSizedOutfitSource Sized;Sized.SourceAsset=Copy;Sized.SourceBodyParts={Body};
  Outfit->Add(Sized,OutfitGuid);
  Copy->RemoveFromRoot();
 }
 const FString OAName=TEXT("OA_outfit_64_NoSunglasses");
 auto* OA=NewObject<UChaosOutfitAsset>(CreatePackage(*(Folder+OAName)),*OAName,RF_Public|RF_Standalone);OA->AddToRoot();
 OA->Build(Outfit);FAssetRegistryModule::AssetCreated(OA);Save(OA);Outfit->RemoveFromRoot();
 const FString WIName=TEXT("WI_OA_outfit_64_NoSunglasses");
 auto* WI=NewObject<UMetaHumanWardrobeItem>(CreatePackage(*(Folder+WIName)),*WIName,RF_Public|RF_Standalone);WI->AddToRoot();
 WI->PrincipalAsset=OA;
 auto* Pipeline=NewObject<UMetaHumanOutfitPipeline>(WI);
 WI->SetPipeline(Pipeline);
 check(Pipeline->GetEditorPipeline());
 WI->ThumbnailName=FText::FromString(TEXT("Outfit 64 - No Sunglasses"));
 FAssetRegistryModule::AssetCreated(WI);Save(WI);
 for(auto* A:Targets)
 {
  UMetaHumanCollection* Collection=A->GetMutableInternalCollection();
  FMetaHumanPaletteItemKey Key;check(Collection->TryAddItemFromWardrobeItem(TEXT("Outfits"),WI,Key));
  Instance(Collection)->SetSingleSlotSelection(TEXT("Outfits"),Key);
  A->WardrobeIndividualAssets.FindOrAdd(TEXT("Outfits")).Items.AddUnique(WI);
  Save(A);A->RemoveFromRoot();
  UE_LOG(LogTemp,Display,TEXT("NO_GLASSES_APPLIED %s"),*A->GetName());
 }
 WI->RemoveFromRoot();OA->RemoveFromRoot();
}
static void Verify()
{
 int32 Failures=0;
 auto* WI=LoadObject<UMetaHumanWardrobeItem>(nullptr,*(Folder+TEXT("WI_OA_outfit_64_NoSunglasses")));
 if(!WI||!WI->GetPipeline()||!WI->GetPipeline()->IsA<UMetaHumanOutfitPipeline>()||!WI->GetEditorPipeline())++Failures;
 auto* OA=LoadObject<UChaosOutfitAsset>(nullptr,*(Folder+TEXT("OA_outfit_64_NoSunglasses")));
 if(WI&&WI->PrincipalAsset.Get()!=OA)++Failures;
 if(!OA||OA->GetMaterials().Num()!=10)++Failures;
 if(OA)for(const auto& M:OA->GetMaterials())if(!M.MaterialInterface||IsGlasses(M.MaterialInterface->GetPathName()))++Failures;
 for(const TCHAR* Size:{TEXT("a"),TEXT("b")})
 {
  const auto* CA=LoadObject<UChaosClothAsset>(nullptr,*(Folder+TEXT("CA_")+Size+TEXT("_NoSunglasses")));
  if(!CA){++Failures;continue;}
  if(CA->GetDataflow())++Failures;
  for(const auto& Data:CA->GetClothCollections())
  {
   UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(Data);
   int32 Vertices=0;
   for(int32 Index=0;Index<Cloth.GetNumRenderPatterns();++Index)
   {auto Pattern=Cloth.GetRenderPattern(Index);Vertices+=Pattern.GetNumRenderVertices();if(IsGlasses(Pattern.GetRenderMaterialSoftObjectPathName().ToString()))++Failures;}
   if(Cloth.GetNumRenderPatterns()!=5||Vertices!=22583)++Failures;
   UE_LOG(LogTemp,Display,TEXT("NO_GLASSES_GEOMETRY size=%s patterns=%d vertices=%d"),Size,Cloth.GetNumRenderPatterns(),Vertices);
  }
 }
 for(const TCHAR* Name:Characters)
 {
  auto* A=LoadObject<UMetaHumanCharacter>(nullptr,*(FString(TEXT("/Game/Fab/MetaHuman/"))+Name));
  if(!A){++Failures;continue;}
  UMetaHumanCollection* Collection=A->GetMutableInternalCollection();FMetaHumanPaletteItemKey Key;FMetaHumanCharacterPaletteItem Item;
  bool Valid=Instance(Collection)->TryGetAnySlotSelection(TEXT("Outfits"),Key)&&Collection->TryFindItem(Key,Item)&&Item.WardrobeItem==WI;
  if(!Valid)++Failures;
  UE_LOG(LogTemp,Display,TEXT("NO_GLASSES_CHARACTER %s valid=%d"),Name,Valid);
 }
 if(Failures){UE_LOG(LogTemp,Error,TEXT("NO_GLASSES_VERIFY failures=%d"),Failures);}
 else{UE_LOG(LogTemp,Display,TEXT("NO_GLASSES_VERIFY failures=0"));}
}
static FAutoConsoleCommand CreateCommand(TEXT("FPS.Wardrobe.CreateNoSunglasses"),TEXT("Create independent glasses-free outfit and apply to three approved characters"),FConsoleCommandDelegate::CreateStatic(&Create));
static FAutoConsoleCommand VerifyCommand(TEXT("FPS.Wardrobe.VerifyNoSunglasses"),TEXT("Verify saved geometry, pipelines and selections"),FConsoleCommandDelegate::CreateStatic(&Verify));
static void RebuildSizes()
{
 auto* OA=LoadObject<UChaosOutfitAsset>(nullptr,*(Folder+TEXT("OA_outfit_64_NoSunglasses")));check(OA);
 auto* Outfit=NewObject<UChaosOutfit>();Outfit->AddToRoot();const FGuid Guid=FGuid::NewGuid();
 for(const TCHAR* Size:{TEXT("a"),TEXT("b")})
 {
  auto* CA=LoadObject<UChaosClothAsset>(nullptr,*(Folder+TEXT("CA_")+Size+TEXT("_NoSunglasses")));check(CA);
  const FString BodyPath=FString(TEXT("/Game/Fab/Mercenary/outfit_64/Meshes/SM_body_"))+Size+TEXT("_CombinedSkelMesh_body_")+Size+TEXT("_CombinedSkelMesh");
  auto* Body=LoadObject<USkeletalMesh>(nullptr,*BodyPath);check(Body);
  FChaosSizedOutfitSource Sized;Sized.SourceAsset=CA;Sized.SourceBodyParts={Body};Outfit->Add(Sized,Guid);
 }
 OA->Build(Outfit);Save(OA);Outfit->RemoveFromRoot();
 UE_LOG(LogTemp,Display,TEXT("NO_GLASSES_SHARED_SIZES_SAVED"));
}
static FAutoConsoleCommand RebuildCommand(TEXT("FPS.Wardrobe.RebuildNoSunglassesSizes"),TEXT("Rebuild only generated outfit using shared size identity"),FConsoleCommandDelegate::CreateStatic(&RebuildSizes));
static void ClearGraphs()
{
 for(const TCHAR* Size:{TEXT("a"),TEXT("b")})
 {
  auto* CA=LoadObject<UChaosClothAsset>(nullptr,*(Folder+TEXT("CA_")+Size+TEXT("_NoSunglasses")));check(CA);
  CA->SetDataflow(nullptr);Save(CA);
 }
}
static FAutoConsoleCommand ClearGraphsCommand(TEXT("FPS.Wardrobe.FinalizeNoSunglasses"),TEXT("Clear source graph only on generated cloth derivatives"),FConsoleCommandDelegate::CreateStatic(&ClearGraphs));
static void Preview(const TArray<FString>& Args)
{
 if(Args.Num()!=1){UE_LOG(LogTemp,Error,TEXT("Specify one approved character"));return;}
 bool Allowed=false;for(const TCHAR* Name:Characters)Allowed|=Args[0]==Name;if(!Allowed)return;
 auto* A=LoadObject<UMetaHumanCharacter>(nullptr,*(FString(TEXT("/Game/Fab/MetaHuman/"))+Args[0]));check(A);A->AddToRoot();
 auto* SS=GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
 if(!SS->TryAddObjectToEdit(A)){UE_LOG(LogTemp,Error,TEXT("NO_GLASSES_PREVIEW_INIT_FAILED %s"),*Args[0]);return;}
 UMetaHumanCollection* C=A->GetMutableInternalCollection();auto Done=MakeShared<bool>(false);
 auto Handle=C->OnPaletteBuilt.AddLambda([Done](EMetaHumanCharacterPaletteBuildQuality Q){if(Q==EMetaHumanCharacterPaletteBuildQuality::Preview)*Done=true;});
 SS->RunCharacterEditorPipelineForPreview(A);
 const double End=FPlatformTime::Seconds()+240.;
 while(!*Done&&FPlatformTime::Seconds()<End&&!IsEngineExitRequested())CommandletHelpers::TickEngine();
 C->OnPaletteBuilt.Remove(Handle);
 bool Valid=*Done;int32 OutfitCount=0;
 if(*Done)for(const auto& Pair:C->GetBuiltData(EMetaHumanCharacterPaletteBuildQuality::Preview).PaletteBuiltData.ItemBuiltData)
 {
  if(const auto* Output=Pair.Value.BuildOutput.GetPtr<FMetaHumanOutfitPipelineBuildOutput>())
  for(const auto& Character:Output->CharacterAssets)
  {
   ++OutfitCount;
   // Preview quality returns an Outfit; only Production returns OutfitMesh.
   UChaosOutfitAsset* Fitted=Character.Value.Outfit;
   const auto* R=Fitted?Fitted->GetResourceForRendering():nullptr;
   Valid&=R&&R->LODRenderData.Num()>0&&R->LODRenderData[0].GetNumVertices()>0;
   Valid&=Fitted&&Fitted->GetOutermost()->GetName()!=Folder+TEXT("OA_outfit_64_NoSunglasses")&&Character.Value.CombinedBodyMesh!=nullptr;
   if(Fitted)for(const auto& Mat:Fitted->GetMaterials())Valid&=Mat.MaterialInterface&&!IsGlasses(Mat.MaterialInterface->GetPathName());
   UE_LOG(LogTemp,Display,TEXT("NO_GLASSES_FITTED %s outfit=%s vertices=%d selected_size=%s"),*Args[0],*GetPathNameSafe(Fitted),R&&R->LODRenderData.Num()?R->LODRenderData[0].GetNumVertices():0,*Character.Value.AutoSelectedSourceSize.ToString());
  }
 }
 Valid&=OutfitCount==1;
 if(Valid){UE_LOG(LogTemp,Display,TEXT("NO_GLASSES_PREVIEW_PASS %s"),*Args[0]);}
 else{UE_LOG(LogTemp,Error,TEXT("NO_GLASSES_PREVIEW_FAILED %s completed=%d outfits=%d"),*Args[0],*Done,OutfitCount);}
 if(*Done){SS->RemoveObjectToEdit(A);A->RemoveFromRoot();}
}
static FAutoConsoleCommand PreviewCommand(TEXT("FPS.Wardrobe.PreviewNoSunglasses"),TEXT("Read-only actual character preview build"),FConsoleCommandWithArgsDelegate::CreateStatic(&Preview));
static void Inspect()
{
 for(const TCHAR* Size:{TEXT("a"),TEXT("b")})
 {
  const FString Path=FString(TEXT("/Game/Fab/Mercenary/outfit_64/ClothAssets/CA_"))+Size;
  const auto* CA=LoadObject<UChaosClothAsset>(nullptr,*Path);check(CA);
  int32 LOD=0;
  for(const auto& Data:CA->GetClothCollections())
  {
   UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(Data);
   for(int32 Index=0;Index<Cloth.GetNumRenderPatterns();++Index)
   {
    auto Pattern=Cloth.GetRenderPattern(Index);
    UE_LOG(LogTemp,Display,TEXT("GLASSES_PATTERN size=%s lod=%d index=%d vertices=%d material=%s"),Size,LOD,Index,Pattern.GetNumRenderVertices(),*Pattern.GetRenderMaterialSoftObjectPathName().ToString());
   }
   ++LOD;
  }
 }
 for(const TCHAR* Size:{TEXT("a"),TEXT("b")})
 {
  const FString Path=FString(TEXT("/Game/Fab/Mercenary/outfit_64/Meshes/"))+(FString(Size)==TEXT("a")?TEXT("A/"):TEXT("B/"))+TEXT("SKM_Sunglasses_")+Size;
  auto* M=LoadObject<USkeletalMesh>(nullptr,*Path);check(M);
  for(const auto& Material:M->GetMaterials())UE_LOG(LogTemp,Display,TEXT("GLASSES_SOURCE %s %s"),Size,*GetPathNameSafe(Material.MaterialInterface));
 }
}
static FAutoConsoleCommand InspectCommand(TEXT("FPS.Wardrobe.Inspect"),TEXT("Read-only outfit pattern inspection"),FConsoleCommandDelegate::CreateStatic(&Inspect));
}
