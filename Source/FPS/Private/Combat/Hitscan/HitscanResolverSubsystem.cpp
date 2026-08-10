#include "Combat/Hitscan/HitscanResolverSubsystem.h"

#include "Combat/Hitscan/HitscanDamageCalculator.h"
#include "Combat/Hitscan/HitscanDamageCore.h"
#include "Combat/Hitscan/HitscanPenetrationCalculator.h"
#include "Combat/Hitscan/HitscanSurfaceDefinition.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Weapon/PenetrableSurfaceComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogHitscanResolver, Log, All);

FHitscanShotResult UHitscanResolverSubsystem::ResolveServerShot(
	const FHitscanShotRequest& Request) const
{
	FHitscanShotResult Result;
	Result.ShotId = Request.ShotId.IsValid() ? Request.ShotId : FGuid::NewGuid();
	Result.InstigatorActor = Request.InstigatorActor;
	Result.CauserActor = Request.CauserActor;
	Result.WeaponTag = Request.WeaponTag;
	Result.ShotIndexInMagazine = Request.ShotIndexInMagazine;

	if (!IsRequestValid(Request))
	{
		return Result;
	}

	FHitscanDamageCoreRequest DamageCoreRequest;
	DamageCoreRequest.DamageSettings = Request.DamageSettings;
	DamageCoreRequest.ExpectedPelletCount = Request.PelletDirections.Num();
	DamageCoreRequest.ImpactSamples.Reserve(Request.PelletDirections.Num());
	Result.PelletResults.Reserve(Request.PelletDirections.Num());

	for (int32 PelletIndex = 0; PelletIndex < Request.PelletDirections.Num(); ++PelletIndex)
	{
		FHitscanPelletTraceResult PelletResult = ResolvePellet(
			Request,
			Request.PelletDirections[PelletIndex],
			PelletIndex);

		DamageCoreRequest.ImpactSamples.Append(PelletResult.ImpactSamples);
		Result.PelletResults.Add(MoveTemp(PelletResult));
	}

	if (!DamageCoreRequest.ImpactSamples.IsEmpty())
	{
		Result.DamageCoreResult = FHitscanDamageCore::ResolveShot(DamageCoreRequest);
	}

	if (Request.TraceSettings.bLogResolvedDamage)
	{
		for (const FHitscanTargetDamageAggregate& TargetAggregate : Result.DamageCoreResult.TargetAggregates)
		{
			UE_LOG(
				LogHitscanResolver,
				Log,
				TEXT("Shot=%s Target=%s Damage=%.2f HeadshotDamage=%.2f Pellets=%d Distance=%.2fm HitZone=%s"),
				*Result.ShotId.ToString(EGuidFormats::DigitsWithHyphens),
				*GetNameSafe(TargetAggregate.TargetActor.Get()),
				TargetAggregate.BaseDamage,
				TargetAggregate.HeadshotDamage,
				TargetAggregate.PelletsHit,
				TargetAggregate.DistanceMeters,
				*TargetAggregate.RepresentativeHitZoneTag.ToString());
		}
	}

	// TODO(GAS 연동): TargetAggregates를 FRogueDamageRequest로 변환해 대상별 한 번 ApplyDamage를 호출한다.
	Result.bProcessed = true;
	return Result;
}

bool UHitscanResolverSubsystem::IsRequestValid(const FHitscanShotRequest& Request) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World)
		|| World->GetNetMode() == NM_Client
		|| !IsValid(Request.InstigatorActor.Get())
		|| !Request.InstigatorActor->HasAuthority()
		|| Request.TraceStart.ContainsNaN()
		|| Request.PelletDirections.IsEmpty()
		|| Request.PelletDirections.Num() > FMath::Max(1, Request.TraceSettings.MaximumPelletCount)
		|| !FMath::IsFinite(Request.TraceSettings.MaximumRangeCentimeters)
		|| Request.TraceSettings.MaximumRangeCentimeters <= 0.0f
		|| Request.TraceSettings.MaximumTraceIterations <= 0
		|| !FMath::IsFinite(Request.TraceSettings.SurfaceExitSearchDistanceCentimeters)
		|| Request.TraceSettings.SurfaceExitSearchDistanceCentimeters <= 0.0f
		|| !FMath::IsFinite(Request.TraceSettings.TraceAdvanceEpsilonCentimeters)
		|| Request.TraceSettings.TraceAdvanceEpsilonCentimeters <= 0.0f
		|| !FMath::IsFinite(Request.TraceSettings.FallbackSurfaceThicknessCentimeters)
		|| Request.TraceSettings.FallbackSurfaceThicknessCentimeters <= 0.0f
		|| !FMath::IsFinite(Request.DamageSettings.BaseDamagePerPellet)
		|| Request.DamageSettings.BaseDamagePerPellet <= 0.0f
		|| !FMath::IsFinite(Request.InitialPenetrationPower)
		|| Request.InitialPenetrationPower < 0.0f)
	{
		return false;
	}

	for (const FVector& PelletDirection : Request.PelletDirections)
	{
		if (PelletDirection.ContainsNaN() || PelletDirection.IsNearlyZero())
		{
			return false;
		}
	}

	return true;
}

FHitscanPelletTraceResult UHitscanResolverSubsystem::ResolvePellet(
	const FHitscanShotRequest& Request,
	const FVector& PelletDirection,
	const int32 PelletIndex) const
{
	FHitscanPelletTraceResult Result;
	Result.PelletIndex = PelletIndex;

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return Result;
	}

	const FVector TraceDirection = PelletDirection.GetSafeNormal();
	if (TraceDirection.IsNearlyZero())
	{
		return Result;
	}

	FHitscanPenetrationState PenetrationState;
	PenetrationState.RemainingPenetrationPower = FMath::Max(0.0f, Request.InitialPenetrationPower);
	PenetrationState.AccumulatedDamageMultiplier = 1.0f;
	PenetrationState.PenetrationCount = 0;

	FVector CurrentTraceStart = Request.TraceStart;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> IgnoredSurfaceComponents;
	TWeakObjectPtr<UPrimitiveComponent> PreviousHitComponent;
	FVector PreviousHitPoint = FVector::ZeroVector;

	for (int32 TraceIteration = 0;
		TraceIteration < Request.TraceSettings.MaximumTraceIterations;
		++TraceIteration)
	{
		const float DistanceFromOriginalStart = FVector::Distance(Request.TraceStart, CurrentTraceStart);
		const float RemainingRangeCentimeters = Request.TraceSettings.MaximumRangeCentimeters
			- DistanceFromOriginalStart;

		if (RemainingRangeCentimeters <= KINDA_SMALL_NUMBER)
		{
			Result.DistanceTraveledCentimeters = Request.TraceSettings.MaximumRangeCentimeters;
			Result.bProcessed = true;
			return Result;
		}

		const FVector TraceEnd = CurrentTraceStart + TraceDirection * RemainingRangeCentimeters;

		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(HitscanResolver_ServerTrace),
			Request.TraceSettings.bTraceComplex,
			Request.CauserActor.Get());
		QueryParams.bReturnPhysicalMaterial = true;
		QueryParams.AddIgnoredActor(Request.InstigatorActor.Get());

		if (IsValid(Request.CauserActor.Get()))
		{
			QueryParams.AddIgnoredActor(Request.CauserActor.Get());
		}

		for (const TObjectPtr<AActor>& IgnoredActor : Request.AdditionalIgnoredActors)
		{
			if (IsValid(IgnoredActor.Get()))
			{
				QueryParams.AddIgnoredActor(IgnoredActor.Get());
			}
		}

		for (const TWeakObjectPtr<UPrimitiveComponent>& IgnoredComponent : IgnoredSurfaceComponents)
		{
			if (IgnoredComponent.IsValid())
			{
				QueryParams.AddIgnoredComponent(IgnoredComponent.Get());
			}
		}

		FHitResult HitResult;
		const bool bBlockingHit = World->LineTraceSingleByChannel(
			HitResult,
			CurrentTraceStart,
			TraceEnd,
			Request.TraceSettings.TraceChannel,
			QueryParams);

		if (Request.TraceSettings.bDrawDebugTrace)
		{
			DrawDebugLine(
				World,
				CurrentTraceStart,
				bBlockingHit ? HitResult.ImpactPoint : TraceEnd,
				bBlockingHit ? FColor::Red : FColor::Green,
				false,
				2.0f,
				0,
				1.0f);
		}

		if (!bBlockingHit)
		{
			Result.DistanceTraveledCentimeters = Request.TraceSettings.MaximumRangeCentimeters;
			Result.bProcessed = true;
			return Result;
		}

		Result.DistanceTraveledCentimeters = FVector::Distance(
			Request.TraceStart,
			HitResult.ImpactPoint);

		UPrimitiveComponent* CurrentHitComponent = HitResult.GetComponent();
		const float LoopGuardDistance = Request.TraceSettings.TraceAdvanceEpsilonCentimeters * 2.0f;
		if (IsValid(CurrentHitComponent)
			&& PreviousHitComponent.Get() == CurrentHitComponent
			&& FVector::DistSquared(PreviousHitPoint, HitResult.ImpactPoint)
				<= FMath::Square(LoopGuardDistance))
		{
			Result.bStoppedByIterationLimit = true;
			Result.bProcessed = true;
			return Result;
		}

		PreviousHitComponent = CurrentHitComponent;
		PreviousHitPoint = HitResult.ImpactPoint;

		if (IsDamageTarget(HitResult, Request.TraceSettings.DamageTarget))
		{
			Result.ImpactSamples.Add(FHitscanDamageCalculator::MakeImpactSample(
				HitResult,
				TraceDirection,
				Result.DistanceTraveledCentimeters / 100.0f,
				PenetrationState.AccumulatedDamageMultiplier,
				PelletIndex));
			Result.bHitDamageTarget = true;
			Result.bProcessed = true;
			return Result;
		}

		FHitscanTraceSurfaceStep& SurfaceStep = Result.SurfaceSteps.AddDefaulted_GetRef();
		SurfaceStep.SurfaceActor = HitResult.GetActor();
		SurfaceStep.SurfaceComponent = HitResult.GetComponent();
		SurfaceStep.EntryPoint = HitResult.ImpactPoint;
		SurfaceStep.SurfaceType = UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());

		if (Request.PenetrationSettings.MaximumPenetrationCount <= 0
			|| PenetrationState.RemainingPenetrationPower <= 0.0f)
		{
			Result.bStoppedBySurface = true;
			Result.bProcessed = true;
			return Result;
		}

		if (!FindSurfaceExit(
			HitResult,
			TraceDirection,
			Request.TraceSettings,
			SurfaceStep.ExitPoint,
			SurfaceStep.ThicknessCentimeters,
			SurfaceStep.bUsedFallbackThickness))
		{
			Result.bStoppedBySurface = true;
			Result.bProcessed = true;
			return Result;
		}

		FHitscanSurfaceSample SurfaceSample;
		SurfaceSample.ThicknessCentimeters = SurfaceStep.ThicknessCentimeters;
		SurfaceSample.SurfaceRule = ResolveSurfaceRule(HitResult, Request);

		const bool bHasConfiguredSurfaceCost =
			SurfaceSample.SurfaceRule.Hardness > KINDA_SMALL_NUMBER
			|| SurfaceSample.SurfaceRule.PenetrationCostPerCentimeter > KINDA_SMALL_NUMBER
			|| SurfaceSample.SurfaceRule.DamageRetentionPerCentimeter < 1.0f - KINDA_SMALL_NUMBER;
		if (!Request.TraceSettings.bAllowZeroCostSurfaceRule
			&& !bHasConfiguredSurfaceCost)
		{
			Result.bStoppedBySurface = true;
			Result.bProcessed = true;
			return Result;
		}

		SurfaceStep.PenetrationResult = FHitscanPenetrationCalculator::ResolveSurface(
			PenetrationState,
			SurfaceSample,
			Request.PenetrationSettings);

		if (!SurfaceStep.PenetrationResult.bPenetrated)
		{
			Result.bStoppedBySurface = true;
			Result.bProcessed = true;
			return Result;
		}

		PenetrationState = SurfaceStep.PenetrationResult.ResultingState;

		if (SurfaceStep.bUsedFallbackThickness)
		{
			if (UPrimitiveComponent* HitComponent = HitResult.GetComponent())
			{
				IgnoredSurfaceComponents.Add(HitComponent);
			}
		}

		CurrentTraceStart = SurfaceStep.ExitPoint
			+ TraceDirection * Request.TraceSettings.TraceAdvanceEpsilonCentimeters;
	}

	Result.bStoppedByIterationLimit = true;
	Result.bProcessed = true;
	return Result;
}

bool UHitscanResolverSubsystem::IsDamageTarget(
	const FHitResult& HitResult,
	const FHitscanDamageTargetSettings& TargetSettings) const
{
	const AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor))
	{
		return false;
	}

	const bool bMatchesActorTag = !TargetSettings.DamageableActorTag.IsNone()
		&& HitActor->ActorHasTag(TargetSettings.DamageableActorTag);

	if (TargetSettings.bRequireDamageableActorTag)
	{
		return bMatchesActorTag;
	}

	const bool bMatchesBoneHit = TargetSettings.bAcceptBoneHits
		&& !HitResult.BoneName.IsNone();
	return bMatchesBoneHit || bMatchesActorTag;
}

bool UHitscanResolverSubsystem::FindSurfaceExit(
	const FHitResult& EntryHit,
	const FVector& TraceDirection,
	const FHitscanTraceSettings& TraceSettings,
	FVector& OutExitPoint,
	float& OutThicknessCentimeters,
	bool& bOutUsedFallbackThickness) const
{
	OutExitPoint = FVector::ZeroVector;
	OutThicknessCentimeters = 0.0f;
	bOutUsedFallbackThickness = false;

	UPrimitiveComponent* HitComponent = EntryHit.GetComponent();
	if (!IsValid(HitComponent) || TraceDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector Direction = TraceDirection.GetSafeNormal();
	const float SearchDistance = FMath::Max(
		TraceSettings.SurfaceExitSearchDistanceCentimeters,
		TraceSettings.TraceAdvanceEpsilonCentimeters * 2.0f);
	const FVector ReverseTraceStart = EntryHit.ImpactPoint + Direction * SearchDistance;
	const FVector ReverseTraceEnd = EntryHit.ImpactPoint
		+ Direction * TraceSettings.TraceAdvanceEpsilonCentimeters;

	FCollisionQueryParams ExitQueryParams(
		SCENE_QUERY_STAT(HitscanResolver_SurfaceExit),
		TraceSettings.bTraceComplex);
	ExitQueryParams.bReturnPhysicalMaterial = false;

	FHitResult ExitHit;
	if (HitComponent->LineTraceComponent(
		ExitHit,
		ReverseTraceStart,
		ReverseTraceEnd,
		ExitQueryParams))
	{
		const float SignedThickness = FVector::DotProduct(
			ExitHit.ImpactPoint - EntryHit.ImpactPoint,
			Direction);

		if (FMath::IsFinite(SignedThickness)
			&& SignedThickness > TraceSettings.TraceAdvanceEpsilonCentimeters)
		{
			OutExitPoint = ExitHit.ImpactPoint;
			OutThicknessCentimeters = SignedThickness;
			return true;
		}
	}

	const float FallbackThickness = ResolveFallbackThickness(EntryHit, TraceSettings);
	if (!FMath::IsFinite(FallbackThickness) || FallbackThickness <= 0.0f)
	{
		return false;
	}

	OutThicknessCentimeters = FallbackThickness;
	OutExitPoint = EntryHit.ImpactPoint + Direction * FallbackThickness;
	bOutUsedFallbackThickness = true;
	return true;
}

FHitscanSurfaceRule UHitscanResolverSubsystem::ResolveSurfaceRule(
	const FHitResult& HitResult,
	const FHitscanShotRequest& Request) const
{
	const TEnumAsByte<EPhysicalSurface> SurfaceType =
		UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());

	FHitscanSurfaceRule SurfaceRule = Request.TraceSettings.DefaultSurfaceRule;
	if (IsValid(Request.SurfaceDefinition.Get()))
	{
		SurfaceRule = Request.SurfaceDefinition->ResolveSurfaceRule(SurfaceType);
	}

	if (const float* HardnessOverride = Request.SurfaceHardnessOverrides.Find(SurfaceType))
	{
		if (FMath::IsFinite(*HardnessOverride) && *HardnessOverride >= 0.0f)
		{
			SurfaceRule.Hardness = *HardnessOverride;
		}
	}

	if (Request.TraceSettings.bUsePenetrableSurfaceComponentOverrides)
	{
		if (const AActor* HitActor = HitResult.GetActor())
		{
			if (const UPenetrableSurfaceComponent* SurfaceComponent =
				HitActor->FindComponentByClass<UPenetrableSurfaceComponent>())
			{
				if (SurfaceComponent->SurfaceHardness > KINDA_SMALL_NUMBER)
				{
					SurfaceRule.Hardness = SurfaceComponent->SurfaceHardness;
				}
			}
		}
	}

	SurfaceRule.Hardness = FMath::Max(0.0f, SurfaceRule.Hardness);
	SurfaceRule.PenetrationCostPerCentimeter = FMath::Max(
		0.0f,
		SurfaceRule.PenetrationCostPerCentimeter);
	SurfaceRule.DamageRetentionPerCentimeter = FMath::Clamp(
		SurfaceRule.DamageRetentionPerCentimeter,
		0.0f,
		1.0f);
	return SurfaceRule;
}

float UHitscanResolverSubsystem::ResolveFallbackThickness(
	const FHitResult& HitResult,
	const FHitscanTraceSettings& TraceSettings) const
{
	if (TraceSettings.bUsePenetrableSurfaceComponentOverrides)
	{
		if (const AActor* HitActor = HitResult.GetActor())
		{
			if (const UPenetrableSurfaceComponent* SurfaceComponent =
				HitActor->FindComponentByClass<UPenetrableSurfaceComponent>())
			{
				if (FMath::IsFinite(SurfaceComponent->PenetrationThickness)
					&& SurfaceComponent->PenetrationThickness > 0.0f)
				{
					return SurfaceComponent->PenetrationThickness;
				}
			}
		}
	}

	return FMath::Max(0.0f, TraceSettings.FallbackSurfaceThicknessCentimeters);
}
