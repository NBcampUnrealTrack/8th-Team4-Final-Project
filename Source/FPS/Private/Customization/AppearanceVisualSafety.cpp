#include "Customization/AppearanceVisualSafety.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GroomComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"

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
	if (Visual->IsA<UGroomComponent>()) Visual->SetVisibility(false, false);
}
