#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"
#include "HitscanResolverSubsystem.generated.h"

class UWorld;
struct FHitscanImpactSample;
struct FRogueHitContext;

// Fired once per accepted server discharge AFTER damage. Misses have an empty actor list.
// References are valid only during this synchronous broadcast; observers must not retain them.
DECLARE_MULTICAST_DELEGATE_TwoParams(FHitscanServerShotResolved,
    const FHitscanShotRequest&, const TArray<TWeakObjectPtr<AActor>>&);

UCLASS()
class FPS_API UHitscanResolverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	bool ResolveServerShot(const FHitscanShotRequest& Request, TArray<FHitscanImpactEffectData>& OutImpactEffects) const;

	FHitscanServerShotResolved OnServerShotResolved;

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
		const TArray<FRogueHitContext>& HitContexts,
		TArray<TWeakObjectPtr<AActor>>* OutAppliedTargets) const;
};
