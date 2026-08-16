#include "HitscanTrace.h"

#include "AbilitySystemInterface.h"
#include "Combat/Hitscan/HitscanBallistics.h"
#include "Combat/Hitscan/HitscanSurfaceDefinition.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Weapon/PenetrableSurfaceComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogHitscanTrace, Log, All);

namespace
{
	enum class EResolvedPelletStopReason : uint8
	{
		Miss,
		RangeLimit,
		SurfaceBlocked,
		LoopGuard,
		IterationLimit
	};

	const TCHAR* ToString(const EResolvedPelletStopReason Reason)
	{
		switch (Reason)
		{
		case EResolvedPelletStopReason::Miss: return TEXT("Miss");
		case EResolvedPelletStopReason::RangeLimit: return TEXT("RangeLimit");
		case EResolvedPelletStopReason::SurfaceBlocked: return TEXT("SurfaceBlocked");
		case EResolvedPelletStopReason::LoopGuard: return TEXT("LoopGuard");
		case EResolvedPelletStopReason::IterationLimit: return TEXT("IterationLimit");
		default: return TEXT("Unknown");
		}
	}

	bool IsPotentialDamageTarget(
		const FHitResult& HitResult,
		const FHitscanDamageTargetSettings& TargetSettings)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!IsValid(HitActor))
		{
			return false;
		}

		const IAbilitySystemInterface* AbilitySystemTarget = Cast<IAbilitySystemInterface>(HitActor);
		const bool bHasAbilitySystem = AbilitySystemTarget
			&& AbilitySystemTarget->GetAbilitySystemComponent() != nullptr;
		const bool bMatchesActorTag = !TargetSettings.DamageableActorTag.IsNone()
			&& HitActor->ActorHasTag(TargetSettings.DamageableActorTag);

		if (TargetSettings.bRequireAbilitySystemTarget)
		{
			return bHasAbilitySystem
				&& (!TargetSettings.bRequireDamageableActorTag || bMatchesActorTag);
		}
		if (TargetSettings.bRequireDamageableActorTag)
		{
			return bMatchesActorTag;
		}

		return bHasAbilitySystem
			|| bMatchesActorTag
			|| (TargetSettings.bAcceptBoneHits && !HitResult.BoneName.IsNone());
	}

	bool TryResolveConfiguredThickness(
		const FHitResult& HitResult,
		const FHitscanTraceSettings& TraceSettings,
		float& OutThicknessCentimeters)
	{
		OutThicknessCentimeters = 0.0f;
		if (!TraceSettings.bUsePenetrableSurfaceComponentOverrides)
		{
			return false;
		}

		const AActor* HitActor = HitResult.GetActor();
		if (!IsValid(HitActor))
		{
			return false;
		}

		const UPenetrableSurfaceComponent* SurfaceComponent =
			HitActor->FindComponentByClass<UPenetrableSurfaceComponent>();
		if (!IsValid(SurfaceComponent)
			|| !FMath::IsFinite(SurfaceComponent->PenetrationThickness)
			|| SurfaceComponent->PenetrationThickness <= 0.0f)
		{
			return false;
		}

		OutThicknessCentimeters = SurfaceComponent->PenetrationThickness;
		return true;
	}

	bool FindSurfaceExit(
		const FHitResult& EntryHit,
		const FVector& TraceDirection,
		const FHitscanTraceSettings& TraceSettings,
		const float RemainingRangeAfterEntryCentimeters,
		FVector& OutExitPoint,
		float& OutThicknessCentimeters)
	{
		OutExitPoint = FVector::ZeroVector;
		OutThicknessCentimeters = 0.0f;

		UPrimitiveComponent* HitComponent = EntryHit.GetComponent();
		if (!IsValid(HitComponent)
			|| TraceDirection.IsNearlyZero()
			|| !FMath::IsFinite(RemainingRangeAfterEntryCentimeters)
			|| RemainingRangeAfterEntryCentimeters <= TraceSettings.TraceAdvanceEpsilonCentimeters)
		{
			return false;
		}

		const FVector Direction = TraceDirection.GetSafeNormal();
		const float Epsilon = TraceSettings.TraceAdvanceEpsilonCentimeters;

		float ConfiguredThickness = 0.0f;
		if (TryResolveConfiguredThickness(EntryHit, TraceSettings, ConfiguredThickness))
		{
			if (ConfiguredThickness + Epsilon > RemainingRangeAfterEntryCentimeters)
			{
				return false;
			}

			OutThicknessCentimeters = ConfiguredThickness;
			OutExitPoint = EntryHit.ImpactPoint + Direction * ConfiguredThickness;
			return true;
		}

		const FBox ComponentBounds = HitComponent->Bounds.GetBox();
		float BoundsExitDistance = 0.0f;
		const bool bHasBoundsExit = HitscanBallistics::CalculateRayExitDistanceFromBox(
			ComponentBounds,
			EntryHit.ImpactPoint,
			Direction,
			BoundsExitDistance);

		auto TryReverseExitTrace = [&](const float SearchDistance) -> bool
		{
			if (!FMath::IsFinite(SearchDistance)
				|| SearchDistance <= Epsilon
				|| SearchDistance > RemainingRangeAfterEntryCentimeters)
			{
				return false;
			}

			const FVector ReverseTraceStart = EntryHit.ImpactPoint + Direction * SearchDistance;
			const FVector ReverseTraceEnd = EntryHit.ImpactPoint + Direction * Epsilon;

			FCollisionQueryParams ExitQueryParams(
				SCENE_QUERY_STAT(HitscanResolver_SurfaceExit),
				TraceSettings.bTraceComplex);
			ExitQueryParams.bReturnPhysicalMaterial = false;

			FHitResult ExitHit;
			if (!HitComponent->LineTraceComponent(
				ExitHit,
				ReverseTraceStart,
				ReverseTraceEnd,
				ExitQueryParams))
			{
				return false;
			}

			const float SignedThickness = FVector::DotProduct(
				ExitHit.ImpactPoint - EntryHit.ImpactPoint,
				Direction);
			if (!FMath::IsFinite(SignedThickness) || SignedThickness <= Epsilon)
			{
				return false;
			}

			OutExitPoint = ExitHit.ImpactPoint;
			OutThicknessCentimeters = SignedThickness;
			return true;
		};

		// 일반 표면은 설정된 탐색 거리 안에서 먼저 찾는다. 사거리 끝 근처라면 남은 거리까지만 검색한다.
		const float PrimarySearchDistance = FMath::Min(
			FMath::Max(TraceSettings.SurfaceExitSearchDistanceCentimeters, Epsilon * 2.0f),
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
			if (ExtendedSearchDistance > PrimarySearchDistance + KINDA_SMALL_NUMBER
				&& TryReverseExitTrace(ExtendedSearchDistance))
			{
				return true;
			}
		}

		if (!TraceSettings.bAllowFallbackSurfaceThickness || !bHasBoundsExit)
		{
			return false;
		}

		// 역트레이스가 모두 실패해도 임의의 10cm로 축소하지 않는다.
		// AABB 출구 거리와 명시 fallback 중 큰 값을 사용해 실제 표면을 얇게 속이는 방향의 오차를 막는다.
		const float SafeFallbackThickness = FMath::Max(
			BoundsExitDistance,
			TraceSettings.FallbackSurfaceThicknessCentimeters);
		if (!FMath::IsFinite(SafeFallbackThickness)
			|| SafeFallbackThickness <= Epsilon
			|| SafeFallbackThickness + Epsilon > RemainingRangeAfterEntryCentimeters)
		{
			return false;
		}

		if (TraceSettings.bLogResolvedDamage)
		{
			UE_LOG(
				LogHitscanTrace,
				Warning,
				TEXT("Surface exit trace failed; using conservative bounds fallback. Actor=%s Component=%s Thickness=%.2fcm"),
				*GetNameSafe(EntryHit.GetActor()),
				*GetNameSafe(HitComponent),
				SafeFallbackThickness);
		}

		OutThicknessCentimeters = SafeFallbackThickness;
		OutExitPoint = EntryHit.ImpactPoint + Direction * SafeFallbackThickness;
		return true;
	}

	FHitscanSurfaceRule ResolveSurfaceRule(
		const FHitResult& HitResult,
		const FHitscanShotRequest& Request)
	{
		const TEnumAsByte<EPhysicalSurface> SurfaceType =
			UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());

		FHitscanSurfaceRule SurfaceRule = Request.TraceSettings.DefaultSurfaceRule;
		if (IsValid(Request.SurfaceDefinition))
		{
			SurfaceRule = Request.SurfaceDefinition->ResolveSurfaceRule(SurfaceType);

			// SurfaceDefinition 쪽 경도가 비어 있을 때만 Weapon/Trace 기본 경도를 fallback으로 사용한다.
			if (SurfaceRule.Hardness <= KINDA_SMALL_NUMBER
				&& Request.TraceSettings.DefaultSurfaceRule.Hardness > KINDA_SMALL_NUMBER)
			{
				SurfaceRule.Hardness = Request.TraceSettings.DefaultSurfaceRule.Hardness;
			}
		}

		if (Request.SurfaceHardnessOverrides)
		{
			if (const float* HardnessOverride = Request.SurfaceHardnessOverrides->Find(SurfaceType))
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
					if (FMath::IsFinite(SurfaceComponent->SurfaceHardness)
						&& SurfaceComponent->SurfaceHardness > KINDA_SMALL_NUMBER)
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

	bool TraceNextBlockingHit(
		UWorld* World,
		const FHitscanShotRequest& Request,
		const FCollisionQueryParams& QueryParams,
		const FVector& TraceStart,
		const FVector& TraceDirection,
		const float RemainingRangeCentimeters,
		FHitResult& OutHitResult)
	{
		const FVector TraceEnd = TraceStart + TraceDirection * RemainingRangeCentimeters;
		const bool bBlockingHit = World->LineTraceSingleByChannel(
			OutHitResult,
			TraceStart,
			TraceEnd,
			Request.TraceSettings.TraceChannel,
			QueryParams);

		if (Request.TraceSettings.bDrawDebugTrace)
		{
			DrawDebugLine(
				World,
				TraceStart,
				bBlockingHit ? OutHitResult.ImpactPoint : TraceEnd,
				bBlockingHit ? FColor::Red : FColor::Green,
				false,
				2.0f,
				0,
				1.0f);
		}

		return bBlockingHit;
	}

	bool TryBuildImpactSample(
		const FHitResult& HitResult,
		const FVector& TraceDirection,
		const FHitscanPenetrationState& PenetrationState,
		const FHitscanDamageTargetSettings& TargetSettings,
		const int32 PelletIndex,
		FHitscanImpactSample& OutImpactSample)
	{
		if (!IsPotentialDamageTarget(HitResult, TargetSettings))
		{
			return false;
		}

		OutImpactSample.TargetActor = HitResult.GetActor();
		OutImpactSample.ImpactPoint = HitResult.ImpactPoint;
		OutImpactSample.BoneName = HitResult.BoneName;
		OutImpactSample.ShotDirection = TraceDirection;
		OutImpactSample.WallRetention = FMath::Clamp(
			PenetrationState.AccumulatedWallRetention,
			0.0f,
			1.0f);
		OutImpactSample.PelletIndex = PelletIndex;
		return true;
	}

	bool TryAdvanceThroughSurface(
		const FHitResult& EntryHit,
		const FVector& TraceDirection,
		const int32 PelletIndex,
		const float RemainingRangeAfterEntryCentimeters,
		const FHitscanShotRequest& Request,
		FHitscanPenetrationState& InOutPenetrationState,
		FVector& OutNextTraceStart)
	{
		if (Request.PenetrationSettings.MaximumPenetrationCount <= 0
			|| InOutPenetrationState.PenetrationCount >= Request.PenetrationSettings.MaximumPenetrationCount
			|| InOutPenetrationState.RemainingPenetrationPower <= 0.0f)
		{
			return false;
		}

		FVector ExitPoint = FVector::ZeroVector;
		float ThicknessCentimeters = 0.0f;
		if (!FindSurfaceExit(
			EntryHit,
			TraceDirection,
			Request.TraceSettings,
			RemainingRangeAfterEntryCentimeters,
			ExitPoint,
			ThicknessCentimeters))
		{
			return false;
		}

		const FHitscanSurfaceRule SurfaceRule = ResolveSurfaceRule(EntryHit, Request);
		const bool bHasConfiguredSurfaceCost =
			SurfaceRule.Hardness > KINDA_SMALL_NUMBER
			|| SurfaceRule.PenetrationCostPerCentimeter > KINDA_SMALL_NUMBER
			|| SurfaceRule.DamageRetentionPerCentimeter < 1.0f - KINDA_SMALL_NUMBER;
		if (!Request.TraceSettings.bAllowZeroCostSurfaceRule && !bHasConfiguredSurfaceCost)
		{
			// 설정 누락 때문에 관통이 조용히 막히지 않게 첫 펠릿은 경고를 남긴다.
			// 디버그 로그를 켠 경우에는 모든 펠릿에서 원인을 확인할 수 있다.
			if (PelletIndex == 0 || Request.TraceSettings.bLogResolvedDamage)
			{
				UE_LOG(
					LogHitscanTrace,
					Warning,
					TEXT("Penetration stopped: surface has zero cost and bAllowZeroCostSurfaceRule is false. Actor=%s Component=%s"),
					*GetNameSafe(EntryHit.GetActor()),
					*GetNameSafe(EntryHit.GetComponent()));
			}
			return false;
		}

		const FHitscanPenetrationStepResult PenetrationResult = HitscanBallistics::ResolvePenetration(
			InOutPenetrationState,
			ThicknessCentimeters,
			SurfaceRule,
			Request.PenetrationSettings);
		if (!PenetrationResult.bPenetrated)
		{
			return false;
		}

		InOutPenetrationState = PenetrationResult.ResultingState;
		OutNextTraceStart = ExitPoint
			+ TraceDirection * Request.TraceSettings.TraceAdvanceEpsilonCentimeters;
		return true;
	}
}

bool HitscanTrace::ResolvePellet(
	UWorld* World,
	const FHitscanShotRequest& Request,
	const FVector& PelletDirection,
	const int32 PelletIndex,
	FHitscanImpactSample& OutImpactSample)
{
	OutImpactSample = FHitscanImpactSample();
	if (!IsValid(World))
	{
		return false;
	}

	const FVector TraceDirection = PelletDirection.GetSafeNormal();
	if (TraceDirection.IsNearlyZero())
	{
		return false;
	}

	auto Stop = [&Request, PelletIndex](const EResolvedPelletStopReason Reason)
	{
		if (Request.TraceSettings.bLogResolvedDamage)
		{
			UE_LOG(
				LogHitscanTrace,
				Verbose,
				TEXT("Pellet stopped. Index=%d Reason=%s"),
				PelletIndex,
				ToString(Reason));
		}
		return false;
	};

	FHitscanPenetrationState PenetrationState;
	PenetrationState.RemainingPenetrationPower = FMath::Max(0.0f, Request.InitialPenetrationPower);
	PenetrationState.AccumulatedWallRetention = 1.0f;

	FVector CurrentTraceStart = Request.TraceStart;
	float PreviousTraceStartProgress = -1.0f;

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(HitscanResolver_ServerTrace),
		Request.TraceSettings.bTraceComplex,
		Request.CauserActor);
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActor(Request.InstigatorActor);
	QueryParams.AddIgnoredActor(Request.CauserActor);

	for (int32 TraceIteration = 0;
		TraceIteration < Request.TraceSettings.MaximumTraceIterations;
		++TraceIteration)
	{
		const float TraceStartProgress = FVector::DotProduct(
			CurrentTraceStart - Request.TraceStart,
			TraceDirection);
		if (TraceIteration > 0
			&& TraceStartProgress <= PreviousTraceStartProgress + KINDA_SMALL_NUMBER)
		{
			return Stop(EResolvedPelletStopReason::LoopGuard);
		}
		PreviousTraceStartProgress = TraceStartProgress;

		const float RemainingRangeCentimeters = Request.TraceSettings.MaximumRangeCentimeters
			- FMath::Max(0.0f, TraceStartProgress);
		if (RemainingRangeCentimeters <= KINDA_SMALL_NUMBER)
		{
			return Stop(EResolvedPelletStopReason::RangeLimit);
		}

		FHitResult HitResult;
		if (!TraceNextBlockingHit(
			World,
			Request,
			QueryParams,
			CurrentTraceStart,
			TraceDirection,
			RemainingRangeCentimeters,
			HitResult))
		{
			return Stop(EResolvedPelletStopReason::Miss);
		}

		// 현재 규칙은 피해 대상에서 펠릿을 종료하고, 월드 표면만 연속 관통한다.
		if (TryBuildImpactSample(
			HitResult,
			TraceDirection,
			PenetrationState,
			Request.TraceSettings.DamageTarget,
			PelletIndex,
			OutImpactSample))
		{
			return true;
		}

		const float DistanceTraveledCentimeters = FVector::Distance(
			Request.TraceStart,
			HitResult.ImpactPoint);
		const float RemainingRangeAfterEntryCentimeters = FMath::Max(
			0.0f,
			Request.TraceSettings.MaximumRangeCentimeters - DistanceTraveledCentimeters);
		if (!TryAdvanceThroughSurface(
			HitResult,
			TraceDirection,
			PelletIndex,
			RemainingRangeAfterEntryCentimeters,
			Request,
			PenetrationState,
			CurrentTraceStart))
		{
			return Stop(EResolvedPelletStopReason::SurfaceBlocked);
		}
	}

	return Stop(EResolvedPelletStopReason::IterationLimit);
}
