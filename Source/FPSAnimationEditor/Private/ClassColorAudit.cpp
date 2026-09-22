#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
static FAutoConsoleCommand ClassColorAudit(TEXT("FPS.ClassColors.Audit"),TEXT("Read-only outfit section bounds"),FConsoleCommandDelegate::CreateLambda([]()
{
 for(const TCHAR* Name:{TEXT("MHC_Frey"),TEXT("MHC_Crowd80")})
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*(FString(TEXT("/Game/MetaHumans/"))+Name+TEXT("/Clothing/")+Name+TEXT("_Outfits")));check(Mesh);
  int32 Index=0;for(const auto& Section:Mesh->GetImportedModel()->LODModels[0].Sections)
  {FBox Bounds(ForceInit);for(const auto& V:Section.SoftVertices)Bounds+=FVector(V.Position);UE_LOG(LogTemp,Display,TEXT("COLOR_SECTION %s section=%d material=%d bounds=%s"),Name,Index++,Section.MaterialIndex,*Bounds.ToString());}
 }
}));
