#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UPrimitiveComponent;

namespace AppearanceVisualSafety
{
	FPS_API bool HasRenderableGeometry(const USkeletalMesh* Mesh);
	// Retire authored visuals only after a valid replacement is ready. Skeletal
	// pose drivers stay registered; obsolete grooms release their runtime resources.
	FPS_API void HideAuthoredVisual(UPrimitiveComponent* Visual);
}
