#include "Combat/Hitscan/HitscanResolverSubsystem.h"

#include "Combat/Hitscan/HitZoneDefinition.h"
#include "Combat/Hitscan/HitscanBallistics.h"
#include "HitscanTrace.h"
#include "Engine/World.h"
#include "GAS/Damage/RogueDamageSubsystem.h"
#include "GameFramework/Actor.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogHitscanResolver, Log, All);

namespace
{
	constexpr int32 MaxTraceIterationsPerPellet = 128;
	constexpr int32 MaxPelletsPerShot = 128;
	constexpr int32 MaxPenetrationsPerPellet = 64;
	constexpr int32 MaxCollisionQueriesPerShot = 4096;
	constexpr int32 MaxCollisionQueriesPerTraceIteration = 3;

	struct FZoneEntry
	{
		FGameplayTag HitZoneTag;
		float BaseDamage = 0.0f;
		float HitZoneMultiplier = 1.0f;
		int32 PelletsHit = 0;
		int32 Priority = 0;
		bool bHeadshot = false;
	};

	struct FTargetEntry
	{
		TWeakObjectPtr<AActor> TargetActor;
		float BaseDamage = 0.0f;
		float TotalDistanceMeters = 0.0f;
		int32 PelletsHit = 0;
		int32 BackstabPelletCount = 0;
		TArray<FZoneEntry> HitZones;
	};

	struct FResolvedTargetDamage
	{
		TWeakObjectPtr<AActor> TargetActor;
		float BaseDamage = 0.0f;
		TArray<FZoneEntry> HitZones;
		FGameplayTag RepresentativeHitZoneTag;
		float DistanceMeters = 0.0f;
		int32 PelletsHit = 0;
		bool bIsBackstab = false;
	};

	bool ValidateShotRequest(const UWorld* World, const FHitscanShotRequest& Request, FString& OutFailureReason)
	{
		auto Fail = [&OutFailureReason](const TCHAR* Reason)
			{
				OutFailureReason = Reason;
				return false;
			};

		if (!IsValid(World))
		{
			return Fail(TEXT("World is invalid"));
		}
		if (World->GetNetMode() == NM_Client)
		{
			return Fail(TEXT("Client attempted authoritative hitscan"));
		}
		if (!IsValid(Request.InstigatorActor))
		{
			return Fail(TEXT("InstigatorActor is invalid"));
		}
		if (!Request.InstigatorActor->HasAuthority())
		{
			return Fail(TEXT("InstigatorActor has no authority"));
		}
		if (!IsValid(Request.CauserActor))
		{
			return Fail(TEXT("CauserActor is invalid"));
		}
		if (Request.TraceStart.ContainsNaN())
		{
			return Fail(TEXT("TraceStart contains NaN"));
		}
		if (Request.PelletDirections.IsEmpty())
		{
			return Fail(TEXT("PelletDirections is empty"));
		}
		if (Request.ShotIndexInMagazine < 0)
		{
			return Fail(TEXT("ShotIndexInMagazine is negative"));
		}

		const FHitscanTraceSettings& Trace = Request.TraceSettings;
		if (Trace.MaximumPelletCount <= 0 || Trace.MaximumPelletCount > MaxPelletsPerShot)
		{
			return Fail(TEXT("MaximumPelletCount is out of bounds"));
		}
		if (Request.PelletDirections.Num() > Trace.MaximumPelletCount)
		{
			return Fail(TEXT("Pellet count exceeds MaximumPelletCount"));
		}
		if (!FMath::IsFinite(Trace.MaximumRangeCentimeters) || Trace.MaximumRangeCentimeters <= 0.0f)
		{
			return Fail(TEXT("MaximumRangeCentimeters must be positive"));
		}
		if (Trace.MaximumTraceIterations <= 0 || Trace.MaximumTraceIterations > MaxTraceIterationsPerPellet)
		{
			return Fail(TEXT("MaximumTraceIterations is out of bounds"));
		}
		const int32 WorstCaseQueriesPerPellet = Trace.MaximumTraceIterations * MaxCollisionQueriesPerTraceIteration;
		if (WorstCaseQueriesPerPellet <= 0
			|| Request.PelletDirections.Num() > MaxCollisionQueriesPerShot / WorstCaseQueriesPerPellet)
		{
			return Fail(TEXT("Worst-case collision queries exceed the per-shot budget"));
		}
		if (!FMath::IsFinite(Trace.SurfaceExitSearchDistanceCentimeters)
			|| Trace.SurfaceExitSearchDistanceCentimeters <= 0.0f)
		{
			return Fail(TEXT("SurfaceExitSearchDistanceCentimeters must be positive"));
		}
		if (!FMath::IsFinite(Trace.TraceAdvanceEpsilonCentimeters)
			|| Trace.TraceAdvanceEpsilonCentimeters <= 0.0f)
		{
			return Fail(TEXT("TraceAdvanceEpsilonCentimeters must be positive"));
		}
		if (Trace.bAllowFallbackSurfaceThickness
			&& (!FMath::IsFinite(Trace.FallbackSurfaceThicknessCentimeters)
				|| Trace.FallbackSurfaceThicknessCentimeters <= 0.0f))
		{
			return Fail(TEXT("FallbackSurfaceThicknessCentimeters must be positive when fallback is enabled"));
		}
		if (!FMath::IsFinite(Trace.DefaultSurfaceRule.Hardness)
			|| Trace.DefaultSurfaceRule.Hardness < 0.0f
			|| !FMath::IsFinite(Trace.DefaultSurfaceRule.PenetrationCostPerCentimeter)
			|| Trace.DefaultSurfaceRule.PenetrationCostPerCentimeter < 0.0f
			|| !FMath::IsFinite(Trace.DefaultSurfaceRule.DamageRetentionPerCentimeter)
			|| Trace.DefaultSurfaceRule.DamageRetentionPerCentimeter < 0.0f
			|| Trace.DefaultSurfaceRule.DamageRetentionPerCentimeter > 1.0f)
		{
			return Fail(TEXT("DefaultSurfaceRule contains invalid values"));
		}

		const FHitscanDamageSettings& Damage = Request.DamageSettings;
		if (!FMath::IsFinite(Damage.BaseDamagePerPellet) || Damage.BaseDamagePerPellet <= 0.0f)
		{
			return Fail(TEXT("BaseDamagePerPellet must be positive"));
		}
		if (!FMath::IsFinite(Damage.DistanceFalloff.EffectiveRangeMeters)
			|| Damage.DistanceFalloff.EffectiveRangeMeters < 0.0f)
		{
			return Fail(TEXT("EffectiveRangeMeters is invalid"));
		}
		if (!FMath::IsFinite(Damage.DistanceFalloff.MaximumRangeMeters)
			|| Damage.DistanceFalloff.MaximumRangeMeters < 0.0f)
		{
			return Fail(TEXT("MaximumRangeMeters is invalid"));
		}
		if (Damage.DistanceFalloff.MaximumRangeMeters + KINDA_SMALL_NUMBER
			< Damage.DistanceFalloff.EffectiveRangeMeters)
		{
			return Fail(TEXT("MaximumRangeMeters must be >= EffectiveRangeMeters"));
		}
		if (!FMath::IsFinite(Damage.DistanceFalloff.MinimumDamageMultiplierAtMaximumRange)
			|| Damage.DistanceFalloff.MinimumDamageMultiplierAtMaximumRange < 0.0f
			|| Damage.DistanceFalloff.MinimumDamageMultiplierAtMaximumRange > 1.0f)
		{
			return Fail(TEXT("MinimumDamageMultiplierAtMaximumRange must be in [0,1]"));
		}
		if (IsValid(Damage.DistanceFalloff.DamageMultiplierCurve.Get())
			&& Damage.DistanceFalloff.EffectiveRangeMeters <= 0.0f)
		{
			return Fail(TEXT("DamageMultiplierCurve requires a positive EffectiveRangeMeters"));
		}
		if (Damage.bEnableBackstab
			&& (!FMath::IsFinite(Damage.BackstabRearConeHalfAngleDegrees)
				|| Damage.BackstabRearConeHalfAngleDegrees < 0.0f
				|| Damage.BackstabRearConeHalfAngleDegrees > 180.0f))
		{
			return Fail(TEXT("BackstabRearConeHalfAngleDegrees must be in [0,180]"));
		}
		if (Damage.Rounding.Mode != EHitscanDamageRoundingMode::None
			&& (!FMath::IsFinite(Damage.Rounding.Step) || Damage.Rounding.Step <= 0.0f))
		{
			return Fail(TEXT("Damage rounding step must be positive"));
		}
		if (Request.HitZoneDamageMultipliers)
		{
			for (const TPair<FGameplayTag, float>& Pair : *Request.HitZoneDamageMultipliers)
			{
				if (!FMath::IsFinite(Pair.Value) || Pair.Value < 0.0f)
				{
					return Fail(TEXT("HitZoneDamageMultipliers contains an invalid multiplier"));
				}
			}
		}

		const FHitscanPenetrationSettings& Penetration = Request.PenetrationSettings;
		if (Penetration.MaximumPenetrationCount < 0
			|| Penetration.MaximumPenetrationCount > MaxPenetrationsPerPellet)
		{
			return Fail(TEXT("MaximumPenetrationCount is out of bounds"));
		}
		if (!FMath::IsFinite(Penetration.MinimumDamageMultiplierToContinue)
			|| Penetration.MinimumDamageMultiplierToContinue < 0.0f
			|| Penetration.MinimumDamageMultiplierToContinue > 1.0f)
		{
			return Fail(TEXT("MinimumDamageMultiplierToContinue must be in [0,1]"));
		}
		if (!FMath::IsFinite(Request.InitialPenetrationPower) || Request.InitialPenetrationPower < 0.0f)
		{
			return Fail(TEXT("InitialPenetrationPower is invalid"));
		}

		if (Request.SurfaceHardnessOverrides)
		{
			for (const TPair<TEnumAsByte<EPhysicalSurface>, float>& Pair : *Request.SurfaceHardnessOverrides)
			{
				if (!FMath::IsFinite(Pair.Value) || Pair.Value < 0.0f)
				{
					return Fail(TEXT("SurfaceHardnessOverrides contains an invalid value"));
				}
			}
		}
		for (const FVector& PelletDirection : Request.PelletDirections)
		{
			if (PelletDirection.ContainsNaN() || PelletDirection.IsNearlyZero())
			{
				return Fail(TEXT("PelletDirections contains an invalid direction"));
			}
		}

		OutFailureReason.Reset();
		return true;
	}

	float ResolveHitZoneMultiplier(
		const FGameplayTag HitZoneTag,
		const TMap<FGameplayTag, float>* Multipliers)
	{
		if (Multipliers)
		{
			if (const float* Multiplier = Multipliers->Find(HitZoneTag))
			{
				return FMath::Max(0.0f, *Multiplier);
			}
		}
		return 1.0f;
	}

	const FZoneEntry* SelectRepresentativeZone(const FTargetEntry& Entry)
	{
		const FZoneEntry* Best = nullptr;
		for (const FZoneEntry& Zone : Entry.HitZones)
		{
			if (!Best
				|| Zone.PelletsHit > Best->PelletsHit
				|| (Zone.PelletsHit == Best->PelletsHit && Zone.BaseDamage > Best->BaseDamage + KINDA_SMALL_NUMBER)
				|| (Zone.PelletsHit == Best->PelletsHit
					&& FMath::IsNearlyEqual(Zone.BaseDamage, Best->BaseDamage)
					&& Zone.Priority > Best->Priority))
			{
				Best = &Zone;
			}
		}
		return Best;
	}

	void BuildTargetAggregates(
		const FHitscanShotRequest& Request,
		const TArray<FHitscanImpactSample>& ImpactSamples,
		TArray<FResolvedTargetDamage>& OutTargetAggregates)
	{
		OutTargetAggregates.Reset();
		if (ImpactSamples.IsEmpty())
		{
			return;
		}

		TArray<FTargetEntry> TargetEntries;
		TMap<TWeakObjectPtr<AActor>, int32> TargetEntryIndices;

		for (const FHitscanImpactSample& Impact : ImpactSamples)
		{
			AActor* TargetActor = Impact.TargetActor.Get();
			if (!IsValid(TargetActor)
				|| Impact.PelletIndex < 0
				|| Impact.PelletIndex >= Request.PelletDirections.Num())
			{
				continue;
			}

			const float BallisticDamage = HitscanBallistics::CalculateBallisticDamage(
				Request.DamageSettings.BaseDamagePerPellet,
				Impact.DistanceMeters,
				Impact.PenetrationDamageMultiplier,
				Request.DamageSettings.DistanceFalloff);
			if (BallisticDamage <= 0.0f)
			{
				continue;
			}

			FGameplayTag HitZoneTag = Request.DamageSettings.DefaultHitZoneTag;
			int32 HitZonePriority = 0;
			bool bHeadshot = false;
			if (IsValid(Request.DamageSettings.HitZoneDefinition.Get()))
			{
				const FHitscanResolvedHitZoneInfo HitZoneInfo =
					Request.DamageSettings.HitZoneDefinition->ResolveHitZoneInfo(
						Impact.BoneName,
						Request.DamageSettings.DefaultHitZoneTag);
				HitZoneTag = HitZoneInfo.HitZoneTag;
				HitZonePriority = HitZoneInfo.Priority;
				bHeadshot = HitZoneInfo.bHeadshot;
			}
			else if (Request.DamageSettings.HeadshotHitZoneTag.IsValid())
			{
				bHeadshot = HitZoneTag.MatchesTagExact(Request.DamageSettings.HeadshotHitZoneTag);
			}

			// 계수는 조회해서 메타데이터로 보관만 한다. 여기서는 절대 피해에 곱하지 않는다.
			const float HitZoneMultiplier = ResolveHitZoneMultiplier(
				HitZoneTag,
				Request.HitZoneDamageMultipliers);

			const TWeakObjectPtr<AActor> TargetKey(TargetActor);
			int32 TargetEntryIndex = INDEX_NONE;
			if (const int32* FoundIndex = TargetEntryIndices.Find(TargetKey))
			{
				TargetEntryIndex = *FoundIndex;
			}
			else
			{
				TargetEntryIndex = TargetEntries.AddDefaulted();
				TargetEntries[TargetEntryIndex].TargetActor = TargetActor;
				TargetEntryIndices.Add(TargetKey, TargetEntryIndex);
			}

			FTargetEntry& TargetEntry = TargetEntries[TargetEntryIndex];
			TargetEntry.PelletsHit++;
			TargetEntry.BaseDamage += BallisticDamage;
			TargetEntry.TotalDistanceMeters += Impact.DistanceMeters;
			if (Request.DamageSettings.bEnableBackstab
				&& HitscanBallistics::IsBackstab(
					TargetActor,
					Impact.ShotDirection,
					Request.DamageSettings.BackstabRearConeHalfAngleDegrees))
			{
				++TargetEntry.BackstabPelletCount;
			}

			FZoneEntry* ZoneEntry = TargetEntry.HitZones.FindByPredicate(
				[HitZoneTag, bHeadshot](const FZoneEntry& Zone)
				{
					// 같은 태그를 여러 Bone 규칙이 공유해도 헤드샷 몫과 일반 몫은 합치지 않는다.
					return Zone.HitZoneTag == HitZoneTag && Zone.bHeadshot == bHeadshot;
				});
			if (!ZoneEntry)
			{
				ZoneEntry = &TargetEntry.HitZones.AddDefaulted_GetRef();
				ZoneEntry->HitZoneTag = HitZoneTag;
				ZoneEntry->HitZoneMultiplier = HitZoneMultiplier;
				ZoneEntry->Priority = HitZonePriority;
				ZoneEntry->bHeadshot = bHeadshot;
			}

			ZoneEntry->BaseDamage += BallisticDamage;
			ZoneEntry->PelletsHit++;
			ZoneEntry->Priority = FMath::Max(ZoneEntry->Priority, HitZonePriority);
		}

		OutTargetAggregates.Reserve(TargetEntries.Num());
		for (const FTargetEntry& Entry : TargetEntries)
		{
			AActor* TargetActor = Entry.TargetActor.Get();
			const int32 PelletsHit = Entry.PelletsHit;
			if (!IsValid(TargetActor)
				|| PelletsHit <= 0
				|| !FMath::IsFinite(Entry.BaseDamage)
				|| !FMath::IsFinite(Entry.TotalDistanceMeters)
				|| Entry.BaseDamage <= 0.0f)
			{
				continue;
			}

			const float RoundedBaseDamage = HitscanBallistics::RoundDamage(
				Entry.BaseDamage,
				Request.DamageSettings.Rounding);
			if (RoundedBaseDamage <= 0.0f)
			{
				continue;
			}

			const float RoundingScale = Entry.BaseDamage > KINDA_SMALL_NUMBER
				? RoundedBaseDamage / Entry.BaseDamage
				: 1.0f;

			FResolvedTargetDamage& Aggregate = OutTargetAggregates.AddDefaulted_GetRef();
			Aggregate.TargetActor = TargetActor;
			Aggregate.BaseDamage = RoundedBaseDamage;
			Aggregate.DistanceMeters = Entry.TotalDistanceMeters / static_cast<float>(PelletsHit);
			Aggregate.PelletsHit = PelletsHit;
			Aggregate.bIsBackstab = Entry.BackstabPelletCount * 2 > PelletsHit;

			Aggregate.HitZones.Reserve(Entry.HitZones.Num());
			float AggregatedZoneDamage = 0.0f;
			for (const FZoneEntry& Zone : Entry.HitZones)
			{
				if (Zone.PelletsHit <= 0 || Zone.BaseDamage <= 0.0f)
				{
					continue;
				}

				FZoneEntry& ZoneAggregate = Aggregate.HitZones.AddDefaulted_GetRef();
				ZoneAggregate = Zone;
				ZoneAggregate.BaseDamage = FMath::Max(0.0f, Zone.BaseDamage * RoundingScale);
				AggregatedZoneDamage += ZoneAggregate.BaseDamage;
			}

			// 반올림으로 생긴 미세 오차만 마지막 항목에 보정한다.
			if (!Aggregate.HitZones.IsEmpty())
			{
				Aggregate.HitZones.Last().BaseDamage = FMath::Max(
					0.0f,
					Aggregate.HitZones.Last().BaseDamage + (RoundedBaseDamage - AggregatedZoneDamage));
			}

			if (const FZoneEntry* Representative = SelectRepresentativeZone(Entry))
			{
				Aggregate.RepresentativeHitZoneTag = Representative->HitZoneTag;
			}
		}
	}

	int32 ApplyResolvedDamage(
		UWorld* World,
		const FHitscanShotRequest& Request,
		const TArray<FResolvedTargetDamage>& TargetAggregates)
	{
		if (!IsValid(World) || TargetAggregates.IsEmpty())
		{
			return 0;
		}

		URogueDamageSubsystem* DamageSubsystem = World->GetSubsystem<URogueDamageSubsystem>();
		if (!IsValid(DamageSubsystem))
		{
			UE_LOG(LogHitscanResolver, Error, TEXT("RogueDamageSubsystem is unavailable."));
			return 0;
		}

		int32 AppliedTargetCount = 0;
		for (const FResolvedTargetDamage& Aggregate : TargetAggregates)
		{
			if (!IsValid(Aggregate.TargetActor.Get()) || Aggregate.BaseDamage <= 0.0f)
			{
				continue;
			}

			FRogueDamageRequest DamageRequest;
			DamageRequest.InstigatorActor = Request.InstigatorActor;
			DamageRequest.TargetActor = Aggregate.TargetActor.Get();
			DamageRequest.CauserActor = Request.CauserActor;
			DamageRequest.SourceTag = RogueTags::Damage_Source_Weapon;
			DamageRequest.WeaponTag = Request.WeaponTag;
			DamageRequest.BaseDamage = Aggregate.BaseDamage;
			DamageRequest.HitZoneTag = Aggregate.RepresentativeHitZoneTag;
			DamageRequest.DistanceMeters = Aggregate.DistanceMeters;
			DamageRequest.bIsBackstab = Aggregate.bIsBackstab;
			DamageRequest.PelletsHit = FMath::Max(1, Aggregate.PelletsHit);
			DamageRequest.ShotIndexInMagazine = Request.ShotIndexInMagazine;
			DamageRequest.HitZoneDamageParts.Reserve(Aggregate.HitZones.Num());

			for (const FZoneEntry& HitZone : Aggregate.HitZones)
			{
				if (HitZone.BaseDamage <= 0.0f)
				{
					continue;
				}

				FRogueHitZoneDamagePart& Part = DamageRequest.HitZoneDamageParts.AddDefaulted_GetRef();
				Part.HitZoneTag = HitZone.HitZoneTag;
				Part.BaseDamage = HitZone.BaseDamage;
				Part.HitZoneMultiplier = HitZone.HitZoneMultiplier;
				Part.PelletsHit = FMath::Max(1, HitZone.PelletsHit);
				Part.bHeadshot = HitZone.bHeadshot;
			}

			const FRogueDamageResult DamageResult = DamageSubsystem->ApplyDamage(DamageRequest);

			if (DamageResult.bApplied)
			{
				++AppliedTargetCount;

				if (Request.TraceSettings.bLogResolvedDamage)
				{
					UE_LOG(
						LogHitscanResolver,
						Log,
						TEXT("Applied hitscan damage. Target=%s WeaponTag=%s BallisticDamage=%.2f HealthDamage=%.2f ShieldDamage=%.2f Zones=%d Pellets=%d Distance=%.2fm"),
						*GetNameSafe(Aggregate.TargetActor.Get()),
						*DamageRequest.WeaponTag.ToString(),
						Aggregate.BaseDamage,
						DamageResult.HealthDamage,
						DamageResult.ShieldDamage,
						DamageRequest.HitZoneDamageParts.Num(),
						Aggregate.PelletsHit,
						Aggregate.DistanceMeters);
				}
			}
			else if (Request.TraceSettings.bLogResolvedDamage)
			{
				UE_LOG(
					LogHitscanResolver,
					Warning,
					TEXT("RogueDamageSubsystem rejected resolved damage. Target=%s BallisticDamage=%.2f"),
					*GetNameSafe(Aggregate.TargetActor.Get()),
					Aggregate.BaseDamage);
			}
		}

		return AppliedTargetCount;
	}
}

bool UHitscanResolverSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer)
		&& IsValid(World)
		&& World->GetNetMode() != NM_Client;
}

FHitscanShotResult UHitscanResolverSubsystem::ResolveServerShot(
	const FHitscanShotRequest& Request) const
{
	FHitscanShotResult Result;
	Result.ShotId = Request.ShotId.IsValid() ? Request.ShotId : FGuid::NewGuid();

	UWorld* World = GetWorld();
	FString FailureReason;
	if (!ValidateShotRequest(World, Request, FailureReason))
	{
		UE_LOG(
			LogHitscanResolver,
			Warning,
			TEXT("Rejected shot. Reason=%s Instigator=%s Causer=%s Pellets=%d"),
			*FailureReason,
			*GetNameSafe(Request.InstigatorActor),
			*GetNameSafe(Request.CauserActor),
			Request.PelletDirections.Num());
		return Result;
	}

	// 상위 흐름은 Request 검증 -> Pellet Trace -> 거리/관통 집계 -> GAS 전달 네 단계로만 유지한다.
	TArray<FHitscanImpactSample> ImpactSamples;
	ImpactSamples.Reserve(Request.PelletDirections.Num());
	for (int32 PelletIndex = 0; PelletIndex < Request.PelletDirections.Num(); ++PelletIndex)
	{
		FHitscanImpactSample ImpactSample;
		if (HitscanTrace::ResolvePellet(
			World,
			Request,
			Request.PelletDirections[PelletIndex],
			PelletIndex,
			ImpactSample))
		{
			ImpactSamples.Add(MoveTemp(ImpactSample));
		}
	}

	TArray<FResolvedTargetDamage> TargetAggregates;
	BuildTargetAggregates(Request, ImpactSamples, TargetAggregates);

	Result.ResolvedTargetCount = TargetAggregates.Num();
	Result.AppliedTargetCount = ApplyResolvedDamage(World, Request, TargetAggregates);
	Result.bProcessed = true;
	return Result;
}