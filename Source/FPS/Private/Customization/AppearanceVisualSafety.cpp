#include "Customization/AppearanceVisualSafety.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GroomComponent.h"
#include "Materials/Material.h"
#include "Rendering/SkeletalMeshRenderData.h"

void AppearanceVisualSafety::UpdateSunglassesDrawOrder(const USkeletalMeshComponent& Face, USkeletalMeshComponent& Outfit)
{
	bool bHasSunglassesLens = false;
	for (int32 Slot = 0; Slot < Outfit.GetNumMaterials(); ++Slot)
	{
		const auto* Material = Outfit.GetMaterial(Slot);
		const auto* Base = Material ? Material->GetMaterial() : nullptr;
		if (!Base) continue;
		const FString Path = Base->GetPathName();
		if (Path == TEXT("/Game/FPS/Customizable/Character/Accessories/M_Ranger_SunglassesLens.M_Ranger_SunglassesLens") ||
			Path == TEXT("/Game/FPS/Customizable/Character/Accessories/M_Vanguard_SunglassesLens.M_Vanguard_SunglassesLens"))
		{
			bHasSunglassesLens = true;
			break;
		}
	}
	// EyeShell and the merged outfit otherwise share priority zero. Drawing the
	// lens after the face prevents eye occlusion translucency appearing over it.
	// Use the smallest offset and reset it on regeneration without sunglasses.
	Outfit.SetTranslucentSortPriority(Face.TranslucencySortPriority + (bHasSunglassesLens ? 1 : 0));
}

bool AppearanceVisualSafety::HasRenderableGeometry(const USkeletalMesh* Mesh)
{
	const auto* RenderData = Mesh ? Mesh->GetResourceForRendering() : nullptr;
	if (!RenderData) return false;
	// Cooked/streamed meshes can omit leading LODs. Require usable geometry,
	// not CPU vertex storage (which packaged builds are allowed to discard).
	for (const auto& LOD : RenderData->LODRenderData)
		if (LOD.GetNumVertices() > 0)
			for (const auto& Section : LOD.RenderSections)
				if (!Section.bDisabled && Section.NumTriangles > 0) return true;
	return false;
}

void AppearanceVisualSafety::HideAuthoredVisual(UPrimitiveComponent* Visual)
{
	if (!Visual) return;
	Visual->SetRenderInMainPass(false);
	Visual->SetRenderInDepthPass(false);
	Visual->SetCastShadow(false);
	Visual->bVisibleInRayTracing = false;
	Visual->ComponentTags.AddUnique(TEXT("AppearanceNoOutline"));
	Visual->SetRenderCustomDepth(false);
	// Strands have their own rendering path and do not obey the main-pass flag.
	// Do not hide the skeletal driver or propagate visibility to replacement children.
	if (auto* Groom = Cast<UGroomComponent>(Visual))
	{
		Groom->SetVisibility(false, false);
		// Called only after the replacement appearance has passed validation.
		// Hiding alone retains per-character strand buffers and Niagara simulation.
		// Keep the authored component/BP intact, but retire its runtime resources.
		Groom->UnregisterComponent();
		Groom->SetComponentTickEnabled(false);
		Groom->SetGroomAsset(nullptr, nullptr);
		Groom->SetGroomCache(nullptr);
		Groom->EmptyOverrideMaterials();
	}
}
