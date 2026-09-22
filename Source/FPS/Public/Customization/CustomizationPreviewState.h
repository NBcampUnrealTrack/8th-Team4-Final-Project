#pragma once
#include "CoreMinimal.h"

// Only the newest successfully displayed generation can be persisted.
inline bool IsPreviewRevisionReady(int32 Requested, int32 Applied, bool Updating)
{
    return Requested > 0 && Applied == Requested && !Updating;
}
