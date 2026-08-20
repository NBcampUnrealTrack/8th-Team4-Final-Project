#include "Combat/LagCompensationRewindScope.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

FLagCompensationRewindScope::FLagCompensationRewindScope(UWorld* World, const FHitscanShotRequest& Request)
{
	if (!IsValid(World) || Request.ClientFireServerTime <= 0.0f)
	{
		return;
	}

	const AGameStateBase* GameState = World->GetGameState();
	if (!IsValid(GameState))
	{
		return;
	}

	RewoundTargets.Reserve(GameState->PlayerArray.Num());
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* TargetPawn = IsValid(PlayerState) ? PlayerState->GetPawn() : nullptr;
		if (!IsValid(TargetPawn) || TargetPawn == Request.InstigatorActor || TargetPawn == Request.CauserActor)
		{
			continue;
		}

		ULagCompensationComponent* LagCompensationComponent =
			TargetPawn->FindComponentByClass<ULagCompensationComponent>();
		if (!IsValid(LagCompensationComponent))
		{
			continue;
		}

		FRewoundTarget RewoundTarget;
		RewoundTarget.LagCompensationComponent = LagCompensationComponent;
		if (!LagCompensationComponent->ApplyFramePackageForTrace(
				Request.ClientFireServerTime,
				Request.TraceSettings.TraceChannel,
				RewoundTarget.RestoreState))
		{
			continue;
		}

		RewoundTargets.Add(MoveTemp(RewoundTarget));
	}

	bActive = !RewoundTargets.IsEmpty();
}

FLagCompensationRewindScope::~FLagCompensationRewindScope()
{
	for (int32 Index = RewoundTargets.Num() - 1; Index >= 0; --Index)
	{
		ULagCompensationComponent* LagCompensationComponent =
			RewoundTargets[Index].LagCompensationComponent.Get();
		if (!IsValid(LagCompensationComponent))
		{
			continue;
		}

		LagCompensationComponent->RestoreAfterTrace(RewoundTargets[Index].RestoreState);
	}
}
