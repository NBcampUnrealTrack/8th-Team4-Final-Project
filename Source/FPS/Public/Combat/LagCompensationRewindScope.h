#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"
#include "Combat/LagCompensationComponent.h"

class FPS_API FLagCompensationRewindScope
{
public:
	FLagCompensationRewindScope(UWorld* World, const FHitscanShotRequest& Request);
	~FLagCompensationRewindScope();

	bool IsActive() const
	{
		return bActive;
	}

private:
	struct FRewoundTarget
	{
		TWeakObjectPtr<ULagCompensationComponent> LagCompensationComponent;
		FLagCompensationRewindRestoreState RestoreState;
	};

	TArray<FRewoundTarget> RewoundTargets;
	bool bActive = false;
};
