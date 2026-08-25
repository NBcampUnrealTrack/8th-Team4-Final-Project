#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"
#include "HitscanResolverSubsystem.generated.h"

class UWorld;
struct FHitscanImpactSample;
struct FRogueHitContext;

UCLASS()
class FPS_API UHitscanResolverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	bool ResolveServerShot(const FHitscanShotRequest& Request, TArray<FHitscanImpactEffectData>& OutImpactEffects) const;

private:
	bool IsValidShotRequest(const UWorld* World, const FHitscanShotRequest& Request) const;
	void ResolveImpactSamples(
		UWorld* World,
		const FHitscanShotRequest& Request,
		TArray<FHitscanImpactSample>& OutImpactSamples,
		TArray<FHitscanImpactEffectData>& OutImpactEffects) const;
	void ApplyDamageToTargets(
		UWorld* World,
		const FHitscanShotRequest& Request,
		const TArray<FRogueHitContext>& HitContexts) const;
};
