#include "Combat/Hitscan/HitscanResolverSubsystem.h"

#include "Combat/Hitscan/HitscanHitContextBuilder.h"
#include "Combat/LagCompensationRewindScope.h"
#include "Engine/World.h"
#include "GAS/Damage/RogueDamageSubsystem.h"
#include "GameFramework/Actor.h"
#include "HitscanTrace.h"
#include "Tags/GameplayTags.h"

bool UHitscanResolverSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer)
		&& IsValid(World)
		&& World->GetNetMode() != NM_Client;
}

bool UHitscanResolverSubsystem::IsValidShotRequest(
	const UWorld* World,
	const FHitscanShotRequest& Request) const
{
	return IsValid(World)
		&& World->GetNetMode() != NM_Client
		&& IsValid(Request.InstigatorActor)
		&& IsValid(Request.CauserActor)
		&& Request.InstigatorActor->HasAuthority()
		&& Request.CauserActor->HasAuthority()
		&& Request.InstigatorActor->GetWorld() == World
		&& Request.CauserActor->GetWorld() == World
		&& !Request.PelletDirections.IsEmpty();
}

void UHitscanResolverSubsystem::ResolveImpactSamples(
	UWorld* World,
	const FHitscanShotRequest& Request,
	TArray<FHitscanImpactSample>& OutImpactSamples) const
{
	if (Request.ClientFireServerTime > 0.0f)
	{
		FLagCompensationRewindScope RewindScope(World, Request);
		if (RewindScope.IsActive())
		{
			FHitscanTrace::ResolveImpacts(World, Request, OutImpactSamples);
			return;
		}
	}

	FHitscanTrace::ResolveImpacts(World, Request, OutImpactSamples);
}

void UHitscanResolverSubsystem::ApplyDamageToTargets(
	UWorld* World,
	const FHitscanShotRequest& Request,
	const TArray<FRogueHitContext>& HitContexts) const
{
	if (!IsValid(World) || HitContexts.IsEmpty())
	{
		return;
	}

	URogueDamageSubsystem* DamageSubsystem = World->GetSubsystem<URogueDamageSubsystem>();
	if (!IsValid(DamageSubsystem))
	{
		return;
	}

	FRogueDamageParams Params;
	Params.Instigator = Request.InstigatorActor;
	Params.Causer = Request.CauserActor;
	Params.WeaponTag = Request.WeaponTag;
	Params.SourceTag = RogueTags::Damage_Source_Weapon;
	Params.ShotIndexInMagazine = Request.ShotIndexInMagazine;

	for (const FRogueHitContext& HitContext : HitContexts)
	{
		DamageSubsystem->ApplyHitscanDamage(Params, HitContext);
	}
}

bool UHitscanResolverSubsystem::ResolveServerShot(
	const FHitscanShotRequest& Request) const
{
	UWorld* World = GetWorld();
	if (!IsValidShotRequest(World, Request))
	{
		return false;
	}

	TArray<FHitscanImpactSample> ImpactSamples;
	ResolveImpactSamples(World, Request, ImpactSamples);

	TArray<FRogueHitContext> HitContexts;
	FHitscanHitContextBuilder::Build(Request, ImpactSamples, HitContexts);

	ApplyDamageToTargets(World, Request, HitContexts);
	return true;
}
