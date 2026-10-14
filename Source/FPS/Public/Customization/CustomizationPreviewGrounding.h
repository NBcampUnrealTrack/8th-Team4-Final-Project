#pragma once

#include "CoreMinimal.h"

inline bool CanReadPreviewVertices(bool bRequiresCookedData, bool bAllowCPUAccess, bool bHasVertexData)
{
    // A cooked buffer can retain a stale pointer after its CPU storage is discarded.
    return bHasVertexData && (!bRequiresCookedData || bAllowCPUAccess);
}

// Vertices are actual reference-pose geometry in the shared model-root space.
// Only the shared root translation may change; authored proportions are retained.
inline bool CalculatePreviewGroundedTransform(TConstArrayView<FVector> Vertices,
    const FTransform& ModelTransform, double FloorZ, FTransform& OutTransform)
{
    OutTransform = ModelTransform;
    if (Vertices.IsEmpty() || ModelTransform.ContainsNaN() || !FMath::IsFinite(FloorZ)) return false;
    double SoleZ = TNumericLimits<double>::Max();
    for (const FVector& Vertex : Vertices)
    {
        if (Vertex.ContainsNaN()) return false;
        const double Z = ModelTransform.TransformPosition(Vertex).Z;
        if (!FMath::IsFinite(Z)) return false;
        SoleZ = FMath::Min(SoleZ, Z);
    }
    FVector Translation = ModelTransform.GetTranslation();
    Translation.Z += FloorZ - SoleZ;
    OutTransform.SetTranslation(Translation);
    return true;
}
