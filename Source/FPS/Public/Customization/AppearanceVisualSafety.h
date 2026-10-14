#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UPrimitiveComponent;
class USkeletalMeshComponent;

namespace AppearanceVisualSafety
{
	FPS_API bool HasRenderableGeometry(const USkeletalMesh* Mesh);
	// Retire authored visuals only after a valid replacement is ready. Skeletal
	// pose drivers stay registered; obsolete grooms release their runtime resources.
	FPS_API void HideAuthoredVisual(UPrimitiveComponent* Visual);
	// Apply after generated outfit materials are assigned; reset when lenses are absent.
	FPS_API void UpdateSunglassesDrawOrder(const USkeletalMeshComponent& Face, USkeletalMeshComponent& Outfit);
}
