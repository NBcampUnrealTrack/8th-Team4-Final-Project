#pragma once
#include "CoreMinimal.h"

namespace FPSFootGrounding
{
inline FVector StablePole(const FVector& Hip,const FVector& Knee,const FVector& Foot,FVector& PreviousDirection)
{
 const FVector Axis=(Foot-Hip).GetSafeNormal(SMALL_NUMBER,FVector::DownVector);
 const FVector Projected=FVector::VectorPlaneProject(Knee-Hip,Axis);
 // A sub-centimetre bend is numerical noise, not a reliable knee direction.
 if(Projected.SizeSquared()>1.f)PreviousDirection=Projected.GetSafeNormal();
 FVector Direction=FVector::VectorPlaneProject(PreviousDirection,Axis).GetSafeNormal();
 if(Direction.IsNearlyZero())Direction=FVector::VectorPlaneProject(FVector::ForwardVector,Axis).GetSafeNormal();
 if(Direction.IsNearlyZero())Direction=FVector::RightVector;
 PreviousDirection=Direction;
 return Hip+Direction*50.f;
}
inline float GroundedWeight(float Previous, bool bGrounded, float DeltaSeconds)
{
 // Never carry an IK pose into the authored jump. Ease in only after landing.
 return bGrounded ? FMath::Lerp(Previous,1.f,1.f-FMath::Exp(-12.f*FMath::Max(0.f,DeltaSeconds))) : 0.f;
}
inline float ContactWeight(float AnimatedLift)
{
 const float T=FMath::Clamp((AnimatedLift-4.f)/8.f,0.f,1.f);
 return 1.f-T*T*(3.f-2.f*T);
}
inline float LimitedOffset(float FloorDelta, float MaxOffset)
{
 return FMath::IsFinite(FloorDelta)?FMath::Clamp(FloorDelta,-FMath::Abs(MaxOffset),FMath::Abs(MaxOffset)):0.f;
}
}
