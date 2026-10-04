#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UPrimitiveComponent;

namespace AppearanceVisualSafety
{
	FPS_API bool HasRenderableGeometry(const USkeletalMesh* Mesh);
	FPS_API void HideAuthoredVisual(UPrimitiveComponent* Visual);
}
