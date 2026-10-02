#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PhysMaterialProbeSubsystem.generated.h"

struct FHitResult;

UCLASS()
class FPS_API UPhysMaterialProbeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool TraceFromView(const FVector& Start, const FVector& End, bool bTraceComplex, const AActor* IgnoredActor, FHitResult& OutHit) const;
	static FString DescribeHit(const FHitResult& Hit, bool bTraceComplex);
	static FString GetSurfaceName(EPhysicalSurface SurfaceType);
};
