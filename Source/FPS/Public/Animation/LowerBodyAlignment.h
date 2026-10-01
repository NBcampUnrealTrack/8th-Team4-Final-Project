#pragma once
#include "CoreMinimal.h"

// Heading is measured relative to each bone's reference-pose basis.
namespace FPSLowerBodyAlignment
{
inline double Heading(const FQuat& Rotation, const FQuat& Reference)
{
 const FVector Forward=(Rotation*Reference.Inverse()).RotateVector(FVector::ForwardVector);
 return FMath::RadiansToDegrees(FMath::Atan2(Forward.Y,Forward.X));
}
inline FTransform AlignPelvis(const FTransform& Pelvis, double Yaw)
{
 FTransform Result=Pelvis;
 Result.SetRotation((FQuat(FVector::UpVector,FMath::DegreesToRadians(Yaw))*Pelvis.GetRotation()).GetNormalized());
 return Result;
}
}
