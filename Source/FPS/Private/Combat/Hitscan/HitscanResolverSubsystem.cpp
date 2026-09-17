#include "Combat/Hitscan/HitscanResolverSubsystem.h"

#include "Combat/Hitscan/HitscanHitContextBuilder.h"
#include "Combat/LagCompensationRewindScope.h"
#include "Engine/World.h"
#include "GAS/Damage/RogueDamageSubsystem.h"
#include "GameFramework/Actor.h"
#include "HitscanTrace.h"
#include "Interfaces/RogueHitscanReactiveTarget.h"
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
	TArray<FHitscanImpactSample>& OutImpactSamples,
	TArray<FHitscanImpactEffectData>& OutImpactEffects) const
{
	if (Request.ClientFireServerTime > 0.0f)
	{
		FLagCompensationRewindScope RewindScope(World, Request);
		if (RewindScope.IsActive())
		{
			FHitscanTrace::ResolveImpacts(World, Request, OutImpactSamples, OutImpactEffects);
			return;
		}
	}

	FHitscanTrace::ResolveImpacts(World, Request, OutImpactSamples, OutImpactEffects);
}

void UHitscanResolverSubsystem::ApplyDamageToTargets(
	UWorld* World,
	const FHitscanShotRequest& Request,
	const TArray<FRogueHitContext>& HitContexts,
	TArray<TWeakObjectPtr<AActor>>* OutAppliedTargets) const
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
		const FRogueDamageResult Result = DamageSubsystem->ApplyHitscanDamage(Params, HitContext);
		if (OutAppliedTargets && Result.bApplied)
		{
			OutAppliedTargets->Add(HitContext.Target.Get());
		}
	}
}

bool UHitscanResolverSubsystem::ResolveServerShot(
	const FHitscanShotRequest& Request,
	TArray<FHitscanImpactEffectData>& OutImpactEffects) const
{
	OutImpactEffects.Reset();

	UWorld* World = GetWorld();
	if (!IsValidShotRequest(World, Request))
	{
		return false;
	}

	TArray<FHitscanImpactSample> ImpactSamples;
	ResolveImpactSamples(World, Request, ImpactSamples, OutImpactEffects);

	// 반응형 사격 과녁은 GAS 대미지 경로와 분리해서 서버 명중만 전달한다.
	TSet<TWeakObjectPtr<AActor>> NotifiedReactiveTargets;
	for (const FHitscanImpactEffectData& ImpactEffect : OutImpactEffects)
	{
		AActor* HitActor = ImpactEffect.HitActor.Get();
		const TWeakObjectPtr<AActor> HitActorKey(HitActor);
		IRogueHitscanReactiveTarget* ReactiveTarget = IsValid(HitActor)
			? Cast<IRogueHitscanReactiveTarget>(HitActor)
			: nullptr;
		if (!ReactiveTarget || NotifiedReactiveTargets.Contains(HitActorKey))
		{
			continue;
		}

		FVector ShotDirection = FVector::ZeroVector;
		if (Request.PelletDirections.IsValidIndex(ImpactEffect.PelletIndex))
		{
			ShotDirection = Request.PelletDirections[ImpactEffect.PelletIndex].GetSafeNormal();
		}

		ReactiveTarget->HandleHitscanImpact(
			Request.InstigatorActor, ImpactEffect.ImpactPoint, ShotDirection);
		NotifiedReactiveTargets.Add(HitActorKey);
	}

	TArray<FRogueHitContext> HitContexts;
	FHitscanHitContextBuilder::Build(Request, ImpactSamples, HitContexts);

	TArray<TWeakObjectPtr<AActor>> AppliedTargets;
	const bool bNotifyObservers = OnServerShotResolved.IsBound();
	ApplyDamageToTargets(World, Request, HitContexts, bNotifyObservers ? &AppliedTargets : nullptr);
	if (bNotifyObservers) { OnServerShotResolved.Broadcast(Request, AppliedTargets); }
	return true;
}
