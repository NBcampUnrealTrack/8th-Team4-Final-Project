#include "HitscanTrace.h"

#include "AbilitySystemInterface.h"
#include "Combat/CombatHitZoneDefinition.h"
#include "Combat/Hitscan/Definitions/HitscanSurfaceDefinition.h"
#include "Combat/Hitscan/Helpers/HitscanMath.h"
#include "Combat/LagCompensationComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/RogueDamageableTarget.h"
#include "Interfaces/RoguePenetrableSurface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Tags/GameplayTags.h"

bool FHitscanTrace::IsPotentialDamageTargetActor(AActor* TargetActor,
												 const FHitscanDamageTargetSettings& TargetSettings)
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const IAbilitySystemInterface* AbilitySystemTarget = Cast<IAbilitySystemInterface>(TargetActor);
	const bool bHasAbilitySystem = AbilitySystemTarget && AbilitySystemTarget->GetAbilitySystemComponent() != nullptr;
	const bool bMatchesActorTag =
		!TargetSettings.DamageableActorTag.IsNone() && TargetActor->ActorHasTag(TargetSettings.DamageableActorTag);

	if (TargetSettings.bRequireAbilitySystemTarget)
	{
		return bHasAbilitySystem && (!TargetSettings.bRequireDamageableActorTag || bMatchesActorTag);
	}
	if (TargetSettings.bRequireDamageableActorTag)
	{
		return bMatchesActorTag;
	}

	return bHasAbilitySystem || bMatchesActorTag;
}

bool FHitscanTrace::IsPotentialDamageTarget(const FHitResult& HitResult,
											const FHitscanDamageTargetSettings& TargetSettings)
{
	return IsPotentialDamageTargetActor(HitResult.GetActor(), TargetSettings);
}

FGameplayTag FHitscanTrace::ResolveHitZoneTag(const FHitResult& HitResult, const FHitscanShotRequest& Request,
											  const FName BoneName)
{
	if (AActor* TargetActor = HitResult.GetActor())
	{
		if (const ULagCompensationComponent* LagCompensationComponent =
				TargetActor->FindComponentByClass<ULagCompensationComponent>())
		{
			FGameplayTag HitZoneTag;
			if (LagCompensationComponent->TryGetHitZoneForComponent(HitResult.GetComponent(), HitZoneTag)
				&& HitZoneTag.IsValid())
			{
				return HitZoneTag;
			}
		}
	}

	FGameplayTag ResolvedHitZoneTag = Request.DamageSettings.DefaultHitZoneTag;

	AActor* TargetActor = HitResult.GetActor();
	if (IsValid(TargetActor) && TargetActor->Implements<URogueDamageableTarget>())
	{
		if (const UCombatHitZoneDefinition* HitZoneDefinition =
				IRogueDamageableTarget::Execute_GetCombatHitZoneDefinition(TargetActor))
		{
			ResolvedHitZoneTag = HitZoneDefinition->ResolveHitZone(BoneName, ResolvedHitZoneTag);
		}
	}

	// Hits against capsules or non-skeletal damageable actors may not have a bone name.
	// HitContextBuilder intentionally rejects invalid zone tags, so an unset weapon fallback would
	// otherwise turn a real blocking hit into an impact-only hit with no damage event.
	// Treat a valid generic damageable hit as torso when no more specific zone can be resolved.
	return ResolvedHitZoneTag.IsValid() ? ResolvedHitZoneTag : RogueTags::Hit_Zone_Torso;
}

bool FHitscanTrace::FindSurfaceExit(const FHitResult& EntryHit, const FVector& TraceDirection,
									const FHitscanTraceSettings& TraceSettings,
									const float RemainingRangeAfterEntryCentimeters, FVector& OutExitPoint)
{
	OutExitPoint = FVector::ZeroVector;

	UPrimitiveComponent* HitComponent = EntryHit.GetComponent();
	if (!IsValid(HitComponent) || TraceDirection.IsNearlyZero() ||
		!FMath::IsFinite(RemainingRangeAfterEntryCentimeters) ||
		RemainingRangeAfterEntryCentimeters <= TraceSettings.TraceAdvanceEpsilonCentimeters)
	{
		return false;
	}

	const FVector Direction = TraceDirection.GetSafeNormal();
	const float Epsilon = TraceSettings.TraceAdvanceEpsilonCentimeters;

	const FBox ComponentBounds = HitComponent->Bounds.GetBox();
	float BoundsExitDistance = 0.0f;
	const bool bHasBoundsExit = FHitscanMath::CalculateRayExitDistanceFromBox(ComponentBounds, EntryHit.ImpactPoint,
																			  Direction, BoundsExitDistance);

	auto TryReverseExitTrace = [&](const float SearchDistance) -> bool
	{
		if (!FMath::IsFinite(SearchDistance) || SearchDistance <= Epsilon ||
			SearchDistance > RemainingRangeAfterEntryCentimeters)
		{
			return false;
		}

		const FVector ReverseTraceStart = EntryHit.ImpactPoint + Direction * SearchDistance;
		const FVector ReverseTraceEnd = EntryHit.ImpactPoint + Direction * Epsilon;

		FCollisionQueryParams ExitQueryParams(SCENE_QUERY_STAT(HitscanResolver_SurfaceExit),
											  TraceSettings.bTraceComplex);
		ExitQueryParams.bReturnPhysicalMaterial = false;

		FHitResult ExitHit;
		if (!HitComponent->LineTraceComponent(ExitHit, ReverseTraceStart, ReverseTraceEnd, ExitQueryParams))
		{
			return false;
		}

		const float SignedExitDistance = FVector::DotProduct(ExitHit.ImpactPoint - EntryHit.ImpactPoint, Direction);
		if (!FMath::IsFinite(SignedExitDistance) || SignedExitDistance <= Epsilon)
		{
			return false;
		}

		OutExitPoint = ExitHit.ImpactPoint;
		return true;
	};

	// 일반 표면은 설정된 탐색 거리 안에서 먼저 찾는다. 사거리 끝 근처라면 남은
	// 거리까지만 검색한다.
	const float PrimarySearchDistance =
		FMath::Min(FMath::Max(TraceSettings.SurfaceExitSearchDistanceCentimeters, Epsilon * 2.0f),
				   RemainingRangeAfterEntryCentimeters);
	if (TryReverseExitTrace(PrimarySearchDistance))
	{
		return true;
	}

	// 300cm보다 두꺼운 표면처럼 첫 역트레이스 시작점이 내부였던 경우에만
	// 컴포넌트 AABB 밖까지 한 번 더 확장해서 실제 출구를 찾는다.
	if (bHasBoundsExit)
	{
		const float ExtendedSearchDistance = BoundsExitDistance + Epsilon * 2.0f;
		if (ExtendedSearchDistance > PrimarySearchDistance + KINDA_SMALL_NUMBER &&
			TryReverseExitTrace(ExtendedSearchDistance))
		{
			return true;
		}
	}

	if (!bHasBoundsExit)
	{
		return false;
	}

	const float SafeExitDistance = BoundsExitDistance;
	if (!FMath::IsFinite(SafeExitDistance) || SafeExitDistance <= Epsilon ||
		SafeExitDistance + Epsilon > RemainingRangeAfterEntryCentimeters)
	{
		return false;
	}

	OutExitPoint = EntryHit.ImpactPoint + Direction * SafeExitDistance;
	return true;
}

FHitscanSurfaceRule FHitscanTrace::ResolveSurfaceRule(const FHitResult& HitResult, const FHitscanShotRequest& Request)
{
	const TEnumAsByte<EPhysicalSurface> SurfaceType =
		UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());

	FHitscanSurfaceRule SurfaceRule = Request.TraceSettings.DefaultSurfaceRule;
	if (IsValid(Request.SurfaceDefinition))
	{
		SurfaceRule = Request.SurfaceDefinition->ResolveSurfaceRule(SurfaceType);
	}

	if (Request.TraceSettings.bUsePenetrableSurfaceOverrides)
	{
		if (AActor* HitActor = HitResult.GetActor())
		{
			if (HitActor->Implements<URoguePenetrableSurface>())
			{
				SurfaceRule.bCanPenetrate = IRoguePenetrableSurface::Execute_CanPenetrate(HitActor);
				SurfaceRule.DamageMultiplier =
					IRoguePenetrableSurface::Execute_GetPenetrationDamageMultiplier(HitActor);
			}
		}
	}

	SurfaceRule.DamageMultiplier = FMath::Clamp(SurfaceRule.DamageMultiplier, 0.0f, 1.0f);
	return SurfaceRule;
}

bool FHitscanTrace::TraceNextBlockingHit(UWorld* World, const FHitscanShotRequest& Request,
										 const FCollisionQueryParams& QueryParams, const FVector& TraceStart,
										 const FVector& TraceDirection, const float RemainingRangeCentimeters,
										 FHitResult& OutHitResult)
{
	const FVector TraceEnd = TraceStart + TraceDirection * RemainingRangeCentimeters;
	const bool bBlockingHit = World->LineTraceSingleByChannel(OutHitResult, TraceStart, TraceEnd,
															  Request.TraceSettings.TraceChannel, QueryParams);

	if (Request.TraceSettings.bDrawDebugTrace)
	{
		DrawDebugLine(World, TraceStart, bBlockingHit ? OutHitResult.ImpactPoint : TraceEnd,
					  bBlockingHit ? FColor::Red : FColor::Green, false, 2.0f, 0, 1.0f);
	}

	return bBlockingHit;
}

bool FHitscanTrace::TryBuildImpactSample(const FHitResult& HitResult, const FVector& TraceDirection,
										 const FHitscanPenetrationState& PenetrationState,
										 const FHitscanShotRequest& Request, const int32 PelletIndex,
										 FHitscanImpactSample& OutImpactSample)
{
	if (!IsPotentialDamageTarget(HitResult, Request.TraceSettings.DamageTarget))
	{
		return false;
	}

	OutImpactSample.TargetActor = HitResult.GetActor();
	OutImpactSample.ImpactPoint = HitResult.ImpactPoint;
	OutImpactSample.ImpactNormal = HitResult.ImpactNormal;
	OutImpactSample.ImpactSurfaceType = UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());
	OutImpactSample.BoneName = HitResult.BoneName;
	OutImpactSample.ShotDirection = TraceDirection;
	OutImpactSample.WallRetention = FMath::Clamp(PenetrationState.AccumulatedWallRetention, 0.0f, 1.0f);
	OutImpactSample.PelletIndex = PelletIndex;
	OutImpactSample.HitZoneTag = ResolveHitZoneTag(HitResult, Request, HitResult.BoneName);
	return true;
}

TEnumAsByte<EPhysicalSurface> FHitscanTrace::ResolveImpactSurfaceType(const FHitResult& HitResult)
{
	if (const AActor* TargetActor = HitResult.GetActor())
	{
		if (TargetActor->Implements<URogueDamageableTarget>())
		{
			// DefaultEngine.ini의 PhysicalSurfaces 매핑상 SurfaceType1 = "Character".
			return SurfaceType1;
		}
	}

	return UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());
}

FHitscanImpactEffectData FHitscanTrace::BuildImpactEffectData(
	const FHitResult& HitResult,
	const int32 PelletIndex)
{
	FHitscanImpactEffectData ImpactEffectData;
	ImpactEffectData.ImpactPoint = HitResult.ImpactPoint;
	ImpactEffectData.ImpactNormal = HitResult.ImpactNormal;
	ImpactEffectData.ImpactSurfaceType = ResolveImpactSurfaceType(HitResult);
	ImpactEffectData.PelletIndex = PelletIndex;
	ImpactEffectData.bHasHit = true;
	return ImpactEffectData;
}

bool FHitscanTrace::TryAdvanceThroughSurface(const FHitResult& EntryHit, const FVector& TraceDirection,
											 const float RemainingRangeAfterEntryCentimeters,
											 const FHitscanShotRequest& Request,
											 FHitscanPenetrationState& InOutPenetrationState,
											 FVector& OutNextTraceStart)
{
	if (Request.PenetrationSettings.MaximumPenetrationCount <= 0 ||
		InOutPenetrationState.PenetrationCount >= Request.PenetrationSettings.MaximumPenetrationCount)
	{
		return false;
	}

	FVector ExitPoint = FVector::ZeroVector;
	if (!FindSurfaceExit(EntryHit, TraceDirection, Request.TraceSettings, RemainingRangeAfterEntryCentimeters,
						 ExitPoint))
	{
		return false;
	}

	const FHitscanSurfaceRule SurfaceRule = ResolveSurfaceRule(EntryHit, Request);
	const FHitscanPenetrationStepResult PenetrationResult = FHitscanMath::ResolvePenetration(
		InOutPenetrationState, SurfaceRule, Request.PenetrationSettings);
	if (!PenetrationResult.bPenetrated)
	{
		return false;
	}

	InOutPenetrationState = PenetrationResult.ResultingState;
	OutNextTraceStart = ExitPoint + TraceDirection * Request.TraceSettings.TraceAdvanceEpsilonCentimeters;
	return true;
}

bool FHitscanTrace::ResolveCurrentPellet(UWorld* World, const FHitscanShotRequest& Request,
										 const FVector& PelletDirection, const int32 PelletIndex,
										 FHitscanImpactSample& OutImpactSample,
										 FHitscanImpactEffectData& OutImpactEffect,
										 bool& bOutHasImpactEffect)
{
	OutImpactSample = FHitscanImpactSample();
	OutImpactEffect = FHitscanImpactEffectData();
	bOutHasImpactEffect = false;

	if (!IsValid(World))
	{
		return false;
	}

	const FVector TraceDirection = PelletDirection.GetSafeNormal();
	if (TraceDirection.IsNearlyZero())
	{
		return false;
	}

	FHitscanPenetrationState PenetrationState;
	PenetrationState.AccumulatedWallRetention = 1.0f;

	FVector CurrentTraceStart = Request.TraceStart;
	float PreviousTraceStartProgress = -1.0f;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HitscanResolver_ServerTrace),
									  Request.TraceSettings.bTraceComplex, Request.CauserActor);
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActor(Request.InstigatorActor);
	QueryParams.AddIgnoredActor(Request.CauserActor);

	for (int32 TraceIteration = 0; TraceIteration < Request.TraceSettings.MaximumTraceIterations; ++TraceIteration)
	{
		const float TraceStartProgress = FVector::DotProduct(CurrentTraceStart - Request.TraceStart, TraceDirection);
		if (TraceIteration > 0 && TraceStartProgress <= PreviousTraceStartProgress + KINDA_SMALL_NUMBER)
		{
			return false;
		}
		PreviousTraceStartProgress = TraceStartProgress;

		const float RemainingRangeCentimeters =
			Request.TraceSettings.MaximumRangeCentimeters - FMath::Max(0.0f, TraceStartProgress);
		if (RemainingRangeCentimeters <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		FHitResult HitResult;
		if (!TraceNextBlockingHit(World, Request, QueryParams, CurrentTraceStart, TraceDirection,
								  RemainingRangeCentimeters, HitResult))
		{
			return false;
		}

		OutImpactEffect = BuildImpactEffectData(HitResult, PelletIndex);
		OutImpactEffect.HitZoneTag = ResolveHitZoneTag(HitResult, Request, HitResult.BoneName);
		bOutHasImpactEffect = true;

		// 현재 규칙은 피해 대상에서 펠릿을 종료하고, 월드 표면만 연속 관통한다.
		if (TryBuildImpactSample(HitResult, TraceDirection, PenetrationState, Request, PelletIndex, OutImpactSample))
		{
			return true;
		}

		const float DistanceTraveledCentimeters = FVector::Distance(Request.TraceStart, HitResult.ImpactPoint);
		const float RemainingRangeAfterEntryCentimeters =
			FMath::Max(0.0f, Request.TraceSettings.MaximumRangeCentimeters - DistanceTraveledCentimeters);
		if (!TryAdvanceThroughSurface(HitResult, TraceDirection, RemainingRangeAfterEntryCentimeters, Request,
									  PenetrationState, CurrentTraceStart))
		{
			return false;
		}
	}

	return false;
}

bool FHitscanTrace::ResolveImpacts(UWorld* World, const FHitscanShotRequest& Request,
								   TArray<FHitscanImpactSample>& OutImpactSamples,
								   TArray<FHitscanImpactEffectData>& OutImpactEffects)
{
	OutImpactSamples.Reset();
	OutImpactEffects.Reset();

	OutImpactSamples.Reserve(Request.PelletDirections.Num());
	OutImpactEffects.Reserve(Request.PelletDirections.Num());
	for (int32 PelletIndex = 0; PelletIndex < Request.PelletDirections.Num(); ++PelletIndex)
	{
		FHitscanImpactSample ImpactSample;
		FHitscanImpactEffectData ImpactEffect;
		bool bHasImpactEffect = false;
		if (ResolveCurrentPellet(
			World,
			Request,
			Request.PelletDirections[PelletIndex],
			PelletIndex,
			ImpactSample,
			ImpactEffect,
			bHasImpactEffect))
		{
			OutImpactSamples.Add(MoveTemp(ImpactSample));
		}

		if (bHasImpactEffect)
		{
			OutImpactEffects.Add(ImpactEffect);
		}
	}

	return !OutImpactSamples.IsEmpty() || !OutImpactEffects.IsEmpty();
}
