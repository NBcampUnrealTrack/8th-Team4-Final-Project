#include "Combat/Hitscan/HitscanResolverSubsystem.h"

#include "Combat/Hitscan/HitZoneDefinition.h"
#include "Combat/Hitscan/HitscanBallistics.h"
#include "Engine/World.h"
#include "GAS/Damage/RogueDamageSubsystem.h"
#include "GameFramework/Actor.h"
#include "HitscanTrace.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogHitscanResolver, Log, All);

namespace
{
	constexpr int32 MaxTraceIterationsPerPellet = 128;
	constexpr int32 MaxPelletsPerShot = 128;
	constexpr int32 MaxPenetrationsPerPellet = 64;
	constexpr int64 MaxCollisionQueriesPerShot = 4096;
	constexpr int64 MaxCollisionQueriesPerTraceIteration = 3;

	struct FTargetHitAccumulator
	{
		FRogueHitContext HitContext;
		int32 FirstPelletIndex = MAX_int32;
		int32 PelletCount = 0;
		int32 BackstabPelletCount = 0;
	};

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}

	bool FailValidation(FString& OutFailureReason, const TCHAR* Reason)
	{
		OutFailureReason = Reason;
		return false;
	}

	bool ValidateShotActorsAndDynamicData(
		const UWorld* World,
		const FHitscanShotRequest& Request,
		FString& OutFailureReason)
	{
		if (!IsValid(World))
		{
			return FailValidation(OutFailureReason, TEXT("World is invalid"));
		}
		if (World->GetNetMode() == NM_Client)
		{
			return FailValidation(OutFailureReason, TEXT("Client attempted authoritative hitscan"));
		}
		if (!IsValid(Request.InstigatorActor)
			|| !Request.InstigatorActor->HasAuthority()
			|| Request.InstigatorActor->GetWorld() != World)
		{
			return FailValidation(OutFailureReason, TEXT("InstigatorActor is not authoritative in this world"));
		}
		if (!IsValid(Request.CauserActor)
			|| !Request.CauserActor->HasAuthority()
			|| Request.CauserActor->GetWorld() != World)
		{
			return FailValidation(OutFailureReason, TEXT("CauserActor is not authoritative in this world"));
		}
		if (!IsFiniteVector(Request.TraceStart))
		{
			return FailValidation(OutFailureReason, TEXT("TraceStart is not finite"));
		}
		if (!Request.bHasDamageDistanceOrigin)
		{
			return FailValidation(OutFailureReason, TEXT("DamageDistanceOrigin was not supplied"));
		}
		if (!IsFiniteVector(Request.DamageDistanceOrigin))
		{
			return FailValidation(OutFailureReason, TEXT("DamageDistanceOrigin is not finite"));
		}
		if (!Request.WeaponTag.IsValid())
		{
			return FailValidation(OutFailureReason, TEXT("WeaponTag is invalid"));
		}
		if (Request.ShotIndexInMagazine < 0)
		{
			return FailValidation(OutFailureReason, TEXT("ShotIndexInMagazine is negative"));
		}
		if (Request.PelletDirections.IsEmpty())
		{
			return FailValidation(OutFailureReason, TEXT("PelletDirections is empty"));
		}

		for (const FVector& PelletDirection : Request.PelletDirections)
		{
			if (!IsFiniteVector(PelletDirection) || PelletDirection.IsNearlyZero())
			{
				return FailValidation(OutFailureReason, TEXT("PelletDirections contains an invalid direction"));
			}
		}

		return true;
	}

	bool ValidateHitResolutionSettings(
		const FHitscanDamageSettings& Settings,
		FString& OutFailureReason)
	{
		if (!Settings.DefaultHitZoneTag.IsValid())
		{
			return FailValidation(OutFailureReason, TEXT("DefaultHitZoneTag is invalid"));
		}
		if (Settings.bEnableBackstab
			&& (!FMath::IsFinite(Settings.BackstabRearConeHalfAngleDegrees)
				|| Settings.BackstabRearConeHalfAngleDegrees < 0.0f
				|| Settings.BackstabRearConeHalfAngleDegrees > 180.0f))
		{
			return FailValidation(OutFailureReason, TEXT("BackstabRearConeHalfAngleDegrees must be in [0,180]"));
		}

		return true;
	}

	bool ValidateTraceSettings(
		const FHitscanShotRequest& Request,
		FString& OutFailureReason)
	{
		const FHitscanTraceSettings& Trace = Request.TraceSettings;
		if (Trace.MaximumPelletCount <= 0 || Trace.MaximumPelletCount > MaxPelletsPerShot)
		{
			return FailValidation(OutFailureReason, TEXT("MaximumPelletCount is out of bounds"));
		}
		if (Request.PelletDirections.Num() > Trace.MaximumPelletCount)
		{
			return FailValidation(OutFailureReason, TEXT("Pellet count exceeds MaximumPelletCount"));
		}
		if (!FMath::IsFinite(Trace.MaximumRangeCentimeters) || Trace.MaximumRangeCentimeters <= 0.0f)
		{
			return FailValidation(OutFailureReason, TEXT("MaximumRangeCentimeters must be positive"));
		}
		if (Trace.MaximumTraceIterations <= 0 || Trace.MaximumTraceIterations > MaxTraceIterationsPerPellet)
		{
			return FailValidation(OutFailureReason, TEXT("MaximumTraceIterations is out of bounds"));
		}

		const int64 WorstCaseQueries = static_cast<int64>(Request.PelletDirections.Num())
			* static_cast<int64>(Trace.MaximumTraceIterations)
			* MaxCollisionQueriesPerTraceIteration;
		if (WorstCaseQueries <= 0 || WorstCaseQueries > MaxCollisionQueriesPerShot)
		{
			return FailValidation(OutFailureReason, TEXT("Worst-case collision queries exceed the per-shot budget"));
		}
		if (!FMath::IsFinite(Trace.SurfaceExitSearchDistanceCentimeters)
			|| Trace.SurfaceExitSearchDistanceCentimeters <= 0.0f)
		{
			return FailValidation(OutFailureReason, TEXT("SurfaceExitSearchDistanceCentimeters must be positive"));
		}
		if (!FMath::IsFinite(Trace.TraceAdvanceEpsilonCentimeters)
			|| Trace.TraceAdvanceEpsilonCentimeters <= 0.0f
			|| Trace.TraceAdvanceEpsilonCentimeters >= Trace.MaximumRangeCentimeters)
		{
			return FailValidation(
				OutFailureReason,
				TEXT("TraceAdvanceEpsilonCentimeters must be positive and smaller than MaximumRangeCentimeters"));
		}
		if (Trace.bAllowFallbackSurfaceThickness
			&& (!FMath::IsFinite(Trace.FallbackSurfaceThicknessCentimeters)
				|| Trace.FallbackSurfaceThicknessCentimeters <= 0.0f))
		{
			return FailValidation(
				OutFailureReason,
				TEXT("FallbackSurfaceThicknessCentimeters must be positive when fallback is enabled"));
		}
		if (!FMath::IsFinite(Trace.DefaultSurfaceRule.Hardness)
			|| Trace.DefaultSurfaceRule.Hardness < 0.0f
			|| !FMath::IsFinite(Trace.DefaultSurfaceRule.PenetrationCostPerCentimeter)
			|| Trace.DefaultSurfaceRule.PenetrationCostPerCentimeter < 0.0f
			|| !FMath::IsFinite(Trace.DefaultSurfaceRule.DamageRetentionPerCentimeter)
			|| Trace.DefaultSurfaceRule.DamageRetentionPerCentimeter < 0.0f
			|| Trace.DefaultSurfaceRule.DamageRetentionPerCentimeter > 1.0f)
		{
			return FailValidation(OutFailureReason, TEXT("DefaultSurfaceRule contains invalid values"));
		}

		if (Request.SurfaceHardnessOverrides)
		{
			for (const TPair<TEnumAsByte<EPhysicalSurface>, float>& Pair : *Request.SurfaceHardnessOverrides)
			{
				if (!FMath::IsFinite(Pair.Value) || Pair.Value < 0.0f)
				{
					return FailValidation(OutFailureReason, TEXT("SurfaceHardnessOverrides contains an invalid value"));
				}
			}
		}

		return true;
	}

	bool ValidatePenetrationSettings(
		const FHitscanShotRequest& Request,
		FString& OutFailureReason)
	{
		const FHitscanPenetrationSettings& Penetration = Request.PenetrationSettings;
		if (Penetration.MaximumPenetrationCount < 0
			|| Penetration.MaximumPenetrationCount > MaxPenetrationsPerPellet)
		{
			return FailValidation(OutFailureReason, TEXT("MaximumPenetrationCount is out of bounds"));
		}
		if (!FMath::IsFinite(Request.InitialPenetrationPower) || Request.InitialPenetrationPower < 0.0f)
		{
			return FailValidation(OutFailureReason, TEXT("InitialPenetrationPower is invalid"));
		}

		return true;
	}

	// 요청과 각 설정 구조의 불변식을 책임 단위로 검증한다.
	bool ValidateShotRequest(
		const UWorld* World,
		const FHitscanShotRequest& Request,
		FString& OutFailureReason)
	{
		OutFailureReason.Reset();
		return ValidateShotActorsAndDynamicData(World, Request, OutFailureReason)
			&& ValidateHitResolutionSettings(Request.DamageSettings, OutFailureReason)
			&& ValidateTraceSettings(Request, OutFailureReason)
			&& ValidatePenetrationSettings(Request, OutFailureReason);
	}

	// 펠릿 결과를 Target별, HitZone별 GAS 입력 구조로 집계한다.
	FGameplayTag ResolveHitZoneTag(const FHitscanShotRequest& Request, const FName BoneName)
	{
		const FHitscanDamageSettings& Settings = Request.DamageSettings;
		return IsValid(Settings.HitZoneDefinition.Get())
			? Settings.HitZoneDefinition->ResolveHitZone(BoneName, Settings.DefaultHitZoneTag)
			: Settings.DefaultHitZoneTag;
	}

	void AccumulateImpactSamples(
		const FHitscanShotRequest& Request,
		const TArray<FHitscanImpactSample>& ImpactSamples,
		TArray<FTargetHitAccumulator>& OutAccumulators)
	{
		OutAccumulators.Reset();
		TMap<TWeakObjectPtr<AActor>, int32> TargetIndices;

		for (const FHitscanImpactSample& Impact : ImpactSamples)
		{
			AActor* TargetActor = Impact.TargetActor.Get();
			if (!IsValid(TargetActor)
				|| Impact.PelletIndex < 0
				|| Impact.PelletIndex >= Request.PelletDirections.Num()
				|| !IsFiniteVector(Impact.ImpactPoint)
				|| !FMath::IsFinite(Impact.WallRetention))
			{
				continue;
			}

			const float WallRetention = FMath::Clamp(Impact.WallRetention, 0.0f, 1.0f);
			if (WallRetention <= 0.0f)
			{
				continue;
			}

			const TWeakObjectPtr<AActor> TargetKey(TargetActor);
			int32 TargetIndex = INDEX_NONE;
			if (const int32* FoundIndex = TargetIndices.Find(TargetKey))
			{
				TargetIndex = *FoundIndex;
			}
			else
			{
				TargetIndex = OutAccumulators.AddDefaulted();
				OutAccumulators[TargetIndex].HitContext.Target = TargetActor;
				TargetIndices.Add(TargetKey, TargetIndex);
			}

			FTargetHitAccumulator& Accumulator = OutAccumulators[TargetIndex];
			++Accumulator.PelletCount;
			if (Request.DamageSettings.bEnableBackstab
				&& HitscanBallistics::IsBackstab(
					TargetActor,
					Impact.ShotDirection,
					Request.DamageSettings.BackstabRearConeHalfAngleDegrees))
			{
				++Accumulator.BackstabPelletCount;
			}

			// 팀 계약의 "첫 명중 펠릿"은 가장 낮은 PelletIndex로 결정한다.
			if (Impact.PelletIndex < Accumulator.FirstPelletIndex)
			{
				Accumulator.FirstPelletIndex = Impact.PelletIndex;
				Accumulator.HitContext.DistanceMeters = FMath::Max(
					0.0f,
					FVector::Distance(Request.DamageDistanceOrigin, Impact.ImpactPoint) / 100.0f);

				Accumulator.HitContext.ShotDirection = Impact.ShotDirection;
				Accumulator.HitContext.HitBoneName = Impact.BoneName;
			}

			const FGameplayTag HitZoneTag = ResolveHitZoneTag(Request, Impact.BoneName);
			FRogueHitGroup* HitGroup = Accumulator.HitContext.HitGroups.FindByPredicate(
				[HitZoneTag](const FRogueHitGroup& Group)
				{
					return Group.HitZoneTag.MatchesTagExact(HitZoneTag);
				});

			if (Request.TraceSettings.bLogResolvedDamage)
			{
				UE_LOG(
					LogHitscanResolver,
					Log,
					TEXT("Hitscan zone resolved. Target=%s Bone=%s HitZone=%s Pellet=%d Retention=%.3f"),
					*GetNameSafe(TargetActor),
					*Impact.BoneName.ToString(),
					*HitZoneTag.ToString(),
					Impact.PelletIndex,
					WallRetention);
			}

			if (!HitGroup)
			{
				HitGroup = &Accumulator.HitContext.HitGroups.AddDefaulted_GetRef();
				HitGroup->HitZoneTag = HitZoneTag;
				HitGroup->WallRetentionSum = 0.0f;
				HitGroup->PelletCount = 0;
			}

			HitGroup->WallRetentionSum += WallRetention;
			++HitGroup->PelletCount;
		}
	}

	void FinalizeTargetHitContexts(
		const FHitscanShotRequest& Request,
		TArray<FTargetHitAccumulator>& Accumulators,
		TArray<FRogueHitContext>& OutHitContexts)
	{
		OutHitContexts.Reset();
		OutHitContexts.Reserve(Accumulators.Num());

		for (FTargetHitAccumulator& Accumulator : Accumulators)
		{
			FRogueHitContext& HitContext = Accumulator.HitContext;
			if (!IsValid(HitContext.Target.Get())
				|| Accumulator.FirstPelletIndex == MAX_int32
				|| !FMath::IsFinite(HitContext.DistanceMeters)
				|| HitContext.DistanceMeters < 0.0f)
			{
				continue;
			}

			HitContext.bBackstab = Request.DamageSettings.bEnableBackstab
				&& Accumulator.PelletCount > 0
				&& Accumulator.BackstabPelletCount * 2 > Accumulator.PelletCount;

			float TotalWallRetention = 0.0f;
			for (int32 GroupIndex = HitContext.HitGroups.Num() - 1; GroupIndex >= 0; --GroupIndex)
			{
				FRogueHitGroup& Group = HitContext.HitGroups[GroupIndex];
				if (!Group.HitZoneTag.IsValid()
					|| Group.PelletCount <= 0
					|| !FMath::IsFinite(Group.WallRetentionSum))
				{
					HitContext.HitGroups.RemoveAtSwap(GroupIndex, 1, EAllowShrinking::No);
					continue;
				}

				Group.WallRetentionSum = FMath::Clamp(
					Group.WallRetentionSum,
					0.0f,
					static_cast<float>(Group.PelletCount));
				TotalWallRetention += Group.WallRetentionSum;
			}

			if (!HitContext.HitGroups.IsEmpty() && TotalWallRetention > 0.0f)
			{
				OutHitContexts.Add(MoveTemp(HitContext));
			}
		}
	}

	// 완성된 Target Context를 팀 대미지 계약으로 전달한다.
	void LogHitContextResult(
		const FHitscanShotRequest& Request,
		const FRogueDamageParams& Params,
		const FRogueHitContext& HitContext,
		const FRogueDamageResult& DamageResult)
	{
		if (!Request.TraceSettings.bLogResolvedDamage)
		{
			return;
		}

		int32 PelletCount = 0;
		float TotalWallRetention = 0.0f;
		for (const FRogueHitGroup& Group : HitContext.HitGroups)
		{
			PelletCount += Group.PelletCount;
			TotalWallRetention += Group.WallRetentionSum;
		}

		if (DamageResult.bApplied)
		{
			UE_LOG(
				LogHitscanResolver,
				Log,
				TEXT("Hitscan context applied. Target=%s WeaponTag=%s Groups=%d Pellets=%d WallRetentionSum=%.3f Distance=%.2fm Backstab=%s"),
				*GetNameSafe(HitContext.Target.Get()),
				*Params.WeaponTag.ToString(),
				HitContext.HitGroups.Num(),
				PelletCount,
				TotalWallRetention,
				HitContext.DistanceMeters,
				HitContext.bBackstab ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(
				LogHitscanResolver,
				Warning,
				TEXT("Hitscan context rejected. Target=%s WeaponTag=%s Groups=%d Pellets=%d WallRetentionSum=%.3f Distance=%.2fm Backstab=%s"),
				*GetNameSafe(HitContext.Target.Get()),
				*Params.WeaponTag.ToString(),
				HitContext.HitGroups.Num(),
				PelletCount,
				TotalWallRetention,
				HitContext.DistanceMeters,
				HitContext.bBackstab ? TEXT("true") : TEXT("false"));
		}
	}

	int32 ApplyDamageToTargets(
		UWorld* World,
		const FHitscanShotRequest& Request,
		const TArray<FRogueHitContext>& HitContexts)
	{
		if (!IsValid(World) || HitContexts.IsEmpty())
		{
			return 0;
		}

		URogueDamageSubsystem* DamageSubsystem = World->GetSubsystem<URogueDamageSubsystem>();
		if (!IsValid(DamageSubsystem))
		{
			UE_LOG(LogHitscanResolver, Error, TEXT("RogueDamageSubsystem is unavailable."));
			return 0;
		}

		FRogueDamageParams Params;
		Params.Instigator = Request.InstigatorActor;
		Params.Causer = Request.CauserActor;
		Params.WeaponTag = Request.WeaponTag;
		Params.SourceTag = RogueTags::Damage_Source_Weapon;
		Params.ShotIndexInMagazine = Request.ShotIndexInMagazine;

		int32 AppliedTargetCount = 0;
		for (const FRogueHitContext& HitContext : HitContexts)
		{
			const FRogueDamageResult DamageResult = DamageSubsystem->ApplyHitscanDamage(Params, HitContext);
			AppliedTargetCount += DamageResult.bApplied ? 1 : 0;
			LogHitContextResult(Request, Params, HitContext, DamageResult);
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
			TEXT("Rejected shot. Reason=%s Instigator=%s Causer=%s WeaponTag=%s Pellets=%d"),
			*FailureReason,
			*GetNameSafe(Request.InstigatorActor),
			*GetNameSafe(Request.CauserActor),
			*Request.WeaponTag.ToString(),
			Request.PelletDirections.Num());
		return Result;
	}

	// 한 발은 요청 검증 -> 펠릿 Trace -> 대상/부위 집계 -> GAS 적용 순서로 처리한다.
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

	TArray<FTargetHitAccumulator> TargetAccumulators;
	AccumulateImpactSamples(Request, ImpactSamples, TargetAccumulators);

	TArray<FRogueHitContext> HitContexts;
	FinalizeTargetHitContexts(Request, TargetAccumulators, HitContexts);

	Result.ResolvedTargetCount = HitContexts.Num();
	Result.AppliedTargetCount = ApplyDamageToTargets(World, Request, HitContexts);
	Result.bProcessed = true;
	return Result;
}